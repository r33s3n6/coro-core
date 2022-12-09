#include "coroutine.h"
#include <utils/log.h>
#include <utils/assert.h>

#include <arch/cpu.h>

#include <task_scheduler.h>

task_queue kernel_task_queue;
task_scheduler kernel_task_scheduler[NCPU];

promise_base::~promise_base() {
    if (_status == init) {
        warnf("promise_base::~promise_base: promise never ran");
        panic("for debug");
    }
}

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

void task_base::resume() { 
    _promise->_status = promise_base::running;
    get_handle().resume();
}

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
        _promise->self_scheduler->__schedule(get_ref());
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


void promise_base::backtrace() {
    promise_base* current_promise = this;
    void* current_await_address = (void*)r_sepc();

    debugf("backtrace:");
    while (current_promise) {
        debugf("    %p: await at %p", current_promise, current_await_address);
        if (!current_promise->caller) {
            break;
        }
        current_await_address = current_promise->await_address;
        current_promise = current_promise->caller.get_promise();
    }

}

task_base task_queue::try_pop() {
    auto guard = make_lock_guard(lock);

    if(queue.empty()){
        // debug_core("task_queue is empty");
        return {};
    }

    task_base ret = std::move(queue.front());
    queue.pop_front();


    return ret;
}

task_base task_queue::pop() {
    lock.lock();

    while(queue.empty()){
        // debugf("task_queue: sleep");
        cpu::my_cpu()->sleep(&wait_task_queue, lock);
    }

    task_base ret = std::move(queue.front());
    queue.pop_front();
    lock.unlock();

    return ret;
}

void task_queue::push(task_base&& proc) {
    auto guard = make_lock_guard(lock);
    queue.push_back(std::move(proc));
    // debugf("task_queue: wake up all (%d)(%d)", queue.size(), wait_task_queue.size());
    
    // wait_task_queue.wake_up_one();

}

void task_scheduler::start() {

    while (true) {
        task_base t = _task_queue->try_pop();
        if (!t) {
            if (return_on_idle) {
                return;
            }
            // uint64 failed_count=0;
            // while (failed_count < 100000000) {
            //     t = _task_queue->try_pop();
            //     if (t) {
            //         break;
            //     }
            //     failed_count++;
            // }
            // if (!t) {
            //     t = _task_queue->pop();
            // }

            cpu::my_cpu()->yield();
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


/*
template <typename return_type>
std::coroutine_handle<> task<return_type>::task_awaiter::await_suspend(std::coroutine_handle<> h) {
    task_base caller(*(std::coroutine_handle<promise_base>*)&h);
    promise_base* caller_promise = caller.get_promise();
    
    p->set_await_address(__builtin_return_address(0));
    {
        auto cpu_ref = cpu::my_cpu();
        last_promise = cpu_ref->get_current_process()->set_promise(p);
    }


    if (!p->self_scheduler) {
        p->self_scheduler = caller_promise->self_scheduler;
    }

    // if we are not set to no_yield, we take same settings as caller
    if(!p->no_yield) {
        p->no_yield = caller_promise->no_yield;
    }

    // we don't have the ownership of the caller
    p->caller = std::move(caller);

    p->set_running();

    // resume ourselves, by tail call optimization, we will not create a new
    // stack frame
    return task::get_handle(p);
}


template <typename return_type>
task<return_type>::ret_opt_type task<return_type>::task_awaiter::await_resume() {

    // alloc_failed_task_shortcut
    if (alloc_fail) {
        return {};
    }

    if (p->get_status() == promise_base::fail) {
        return {};
    }

    {
        auto cpu_ref = cpu::my_cpu();
        cpu_ref->get_current_process()->set_promise(last_promise);
    }

    return std::move(p->result);
}

*/