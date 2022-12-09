#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <coroutine.h>

#include <utils/wait_queue.h>

task<void> __task_executor(promise<void>* p);

class task_queue {
    list<task_base> queue;
    spinlock lock {"task_queue.lock"};
    wait_queue wait_task_queue;

public:
    task_base pop();
    task_base try_pop();
    void push(task_base&& proc);
    bool empty() { 
        return queue.empty();
    }
    int32 size() {
        return queue.size();
    }
    
};


struct task_scheduler {
    // std::deque<task_base> task_queue;
    task_queue* _task_queue;
    bool return_on_idle = false;

    void schedule(task_base&& h) {
        // let them clear themselves on final_suspend
        if(!h){
            return;
        }

        // task_base tb;

        // scheduler takes ownership of the coroutine whose caller is empty
        if(!h.get_promise()->caller){
            // we wrap the task with a task_executor
            task_base buf = std::move(h); // take the ownership of the coroutine
            buf.clear_owner();
            auto task = __task_executor((promise<void>*)buf.get_promise());
            task.clear_owner();
            h = std::move(task);
        }

        h.clear_owner();

        __schedule(std::move(h));

    }

    void __schedule(task_base&& h) {
        h.clear_owner();
        task_base buf = std::move(h);
        buf.get_promise()->self_scheduler = this;
        _task_queue->push(std::move(buf));
    }

    bool is_free() { return _task_queue->empty(); }

    void start();

    void set_queue(task_queue* q) {
        _task_queue = q;
    }
};


struct this_scheduler_t {
    enum class _Construct { _Token };
    explicit constexpr this_scheduler_t(_Construct) {}
    bool await_ready() const { 
        return false; 
    }
    std::coroutine_handle<> await_suspend(task_base h) const {
        promise_base* p = h.get_promise();
        if(p->no_yield) {
            return h.get_handle(); // resume immediately
        }

        task_scheduler* s = p->self_scheduler;

        if (!s || s->is_free()) {
            return h.get_handle(); // resume immediately
        }

        s->schedule(std::move(h));

        // switch back to scheduler
        return std::noop_coroutine();
    }
    void await_resume() const {}
};
inline constexpr this_scheduler_t this_scheduler{
    this_scheduler_t::_Construct::_Token};



struct get_taskbase_t {
    bool await_ready() { return false; }

    task_base _task;
    bool await_suspend(task_base h) {
        _task = std::move(h);
        return false;
    }
    task_base await_resume() {
        return std::move(_task);
    }
};

extern task_queue kernel_task_queue;
extern task_scheduler kernel_task_scheduler[NCPU];

#endif