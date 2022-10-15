#include "coroutine.h"
#include <utils/log.h>

scheduler kernel_scheduler;

task_base::task_base(task_base&& t)
    : _promise(t._promise), alloc_fail(t.alloc_fail), _owner(t._owner) {

    t._promise = nullptr;
    t._owner = false;

}
task_base& task_base::operator=(task_base&& t) {
    this->_promise = t._promise;
    this->alloc_fail = t.alloc_fail;
    this->_owner = t._owner;

    t._promise = nullptr;
    t._owner = false;

    return *this;
}

task_base::task_base(promise_type* p, bool owner)
    : _promise(p), alloc_fail(false), _owner(owner) {}



// task executor destruct ifself after execution
task<void> __task_executor(promise<void>* p) {

    // __printf("task executor: %p\n", p);
    p->has_error_handler = true;
    
    task<void> t{p}; // got the ownership

    co_await t;

    

    if (p->get_status() == promise_base::fail) {
        co_warnf("task_executor failed");
    } 
    co_return task_ok;
}

#ifdef HANDLE_MEMORY_ALLOC_FAIL

task_fail_t promise_base::get_return_object_on_allocation_failure() {
    kernel_console_logger.printf("alloc task failed\n");
    return task_fail;
}
#endif

void scheduler::start() {
    while (true) {
        if (task_queue.empty()) {
            break;
        }
        // all tasks we own, we start it here
        task_base t = std::move(task_queue.front());
        task_queue.pop_front();

        kernel_console_logger.printf(logger::log_level::TRACE,
         "scheduler: try to switch to %p\n", t.get_promise());
        // printf("scheduler: task status: %d\n", t.get_promise()->get_status());

        t.resume();
        // we do not track the status of tasks

    }
}