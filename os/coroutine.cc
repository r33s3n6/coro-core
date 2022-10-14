#include "coroutine.h"


scheduler kernel_scheduler;

task_base::task_base(task_base&& t)
    : _promise(t._promise), alloc_fail(t.alloc_fail) {
    this->_owner = t._owner;
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
    : _promise(p), alloc_fail(false) {
        _owner = owner;
    }

/*
struct simple_awaiter {
    promise<void>* callee;
    bool await_ready() { return false; }
    std::coroutine_handle<> await_suspend(task_base caller) {
        callee->caller = std::move(caller);
        return std::coroutine_handle<promise<void>>::from_promise(*callee);
    }
    bool await_resume() {
        if (callee->get_status() == promise_base::fail) {
            return false;
        }
        return callee->result;
    }

};*/

// task executor destruct ifself after execution
task<void> __task_executor(promise<void>* p) {
    p->has_error_handler = true;

    task<void> t{p}; // got the ownership

    bool result = co_await t;

    if (result) {
        co_return task_ok;
    } else {
        __printf("task_executor failed\n");
        co_return task_fail;
    }
}