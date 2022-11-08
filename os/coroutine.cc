#include "coroutine.h"
#include <utils/log.h>
#include <utils/assert.h>

task_queue kernel_task_queue;
task_scheduler kernel_task_scheduler[NCPU];

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

void task_base::wake_up(){
    if(_promise->self_scheduler){
        _promise->self_scheduler->schedule(get_ref());
    } else {
        kernel_task_queue.push(get_ref());
    }
    
}



#ifdef HANDLE_MEMORY_ALLOC_FAIL

task_fail_t promise_base::get_return_object_on_allocation_failure() {
    kernel_console_logger.printf("alloc task failed\n");
    return task_fail;
}
#endif

task_base task_queue::pop() {
    auto guard = make_lock_guard(lock);

    if(queue.empty()){
        // debug_core("task_queue is empty");
        return {};
    }

    task_base ret = std::move(queue.front());
    queue.pop_front();


    return ret;
}

void task_queue::push(task_base&& proc) {
    auto guard = make_lock_guard(lock);
    queue.push_back(std::move(proc));

}

void task_scheduler::start() {
    while (true) {

        
        task_base t = _task_queue->pop();
        if (!t) {
            // debugf("task_scheduler: no task to run, yield");

            cpu::my_cpu()->yield();
            // TODO replace with sleep

            // debugf("task_scheduler: yield done");
            continue;
        }

        
        // debug_core("task_scheduler: try to switch to %p\n", t.get_promise());
        // printf("scheduler: task status: %d\n", t.get_promise()->get_status());
        kernel_assert(cpu::local_irq_on(), "task_scheduler: irq off");
        // all tasks we own, we start it here
        t.resume();

        // debugf("task_scheduler: switch done");
        // we do not track the status of tasks

    }
}