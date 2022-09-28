#ifndef COROUTINE_H
#define COROUTINE_H

#include <coroutine>
#include <optional>
#include <queue>

#include "types.h"
#include "mm/allocator.h"
#include "log/printf.h"



struct scheduler;
extern scheduler kernel_scheduler;



template <typename T>
struct storage {
    using opt_type = std::optional<T>;
    opt_type value;
    operator opt_type() { 
        return value;
    }
    explicit operator bool() { return value.has_value(); }
    storage() {}
    storage(const opt_type& v) : value(v) {
    }
};

template <>
struct storage<void> {
    bool value;
    operator bool() { return value; }

    storage(bool v = false) : value(v) {}
};
struct promise_base;

template <typename return_type = void>
struct promise;

struct task_ok_t {
    enum class _Construct { _Token };
    explicit constexpr task_ok_t(_Construct) {}
};
inline constexpr task_ok_t task_ok{task_ok_t::_Construct::_Token};

struct task_fail_t {
    enum class _Construct { _Token };
    explicit constexpr task_fail_t(_Construct) {}
};
inline constexpr task_fail_t task_fail{task_fail_t::_Construct::_Token};


struct task_base : std::coroutine_handle<promise_base> {
    using promise_type = promise_base;
    task_base() {}
    task_base(std::coroutine_handle<promise_base> handle)
        : std::coroutine_handle<promise_base>(handle) {}
    
    template <typename T>
    task_base(std::coroutine_handle<T> h)  {
        static_assert(std::is_base_of_v<promise_base, T>);
        *this = std::coroutine_handle<promise_base>::from_promise(h.promise());
    }
};

template <typename return_type = void>
struct task : std::coroutine_handle<promise<return_type>> {
    using promise_type = promise<return_type>;
    using opt_type = storage<return_type>;

    bool fail = false;
    bool destroyed = false;
    

    task() {}

    task(task_fail_t t) { fail = true; }

    task(std::coroutine_handle<promise<return_type>> handle)
        : std::coroutine_handle<promise<return_type>>(handle) {}

    


    ~task() {
        //if(this->promise().done) {
        //    printf("task %p:promise done, destruct: %p\n", this, &this->promise());
        //    this->destroy();
        //}
    }

    operator task_base() {
        return task_base::from_promise(this->promise());
    }

    void value_generated(opt_type&& result) {
        this->result = std::forward(result);
    }
    bool await_ready() { return fail; }
    // we want to get the result from co_yield/co_return
    //template <typename T>
    bool await_suspend(task_base caller);

    opt_type await_resume();


};

struct promise_base {
    enum status : uint8_t { suspend, zombie, done, fail };
    task_base caller;
    status status = suspend;
    scheduler* self_scheduler=nullptr;

    bool fast_fail = true;

};

template <typename return_type>
struct promise : promise_base {
    using task_type = task<return_type>;
    using opt_type = task_type::opt_type;

    opt_type result;
    
    // std::coroutine_handle<> fast_fail_handle;

    // scheduler* caller_scheduler=nullptr;

    promise(){
        
    }
    ~promise() {
        printf("promise %p: destruct\n", this);
    }

    task_type get_return_object() { return {task_type::from_promise(*this)}; }

    static task_type get_return_object_on_allocation_failure() {

        return {task_fail};
    }

    struct suspend {
        bool _suspend;
        suspend(bool suspend = true) : _suspend(suspend) {}
        bool await_ready() { return !_suspend; }
        void await_suspend(std::coroutine_handle<>) {}
        void await_resume() {}
    };

    std::suspend_always initial_suspend() noexcept { return {}; }
    suspend final_suspend() noexcept;

    void unhandled_exception() {}

    void set_return_value(const opt_type& result) {
        printf("%p: set return value\n", this);
        this->result = result;
        if(this->result)
            if(this->caller)
                this->status = zombie;
            else
                this->status = done;
        else
            this->status = fail;
        
    }

    template <std::convertible_to<return_type> from_t, typename T = return_type>
    void return_value(from_t&& from,
                      std::enable_if_t<!std::is_same_v<T, void>, void*> = 0) {
        set_return_value({std::forward<return_type>(from)});
    }
    void return_value(task_fail_t t) { 
        printf("task %p return fail\n", this);
        set_return_value({});    
    }

    template <typename T = return_type>
    void return_value(std::enable_if_t<std::is_same_v<T, void>, task_ok_t> t) {
        set_return_value({true});
    }

    // struct resume_caller_t {
    //     std::coroutine_handle<> caller;
    //     resume_caller_t(std::coroutine_handle<> caller)
    //         : caller(caller) {}
    //     bool await_ready() { return false; }
    //     void await_suspend(std::coroutine_handle<> h) {
    //         return caller;
    //     }
    //     void await_resume() {}
    // };

    template <std::convertible_to<return_type> from_t>
    std::suspend_always yield_value(from_t&& from);

    template <typename U>
    U&& await_transform(U&& awaitable) noexcept
    {
        return static_cast<U&&>(awaitable);
    }
};

struct scheduler {

    std::deque<task_base> task_queue;

    // TODO: may schedule to different scheduler
    struct next_schedule_t {
        scheduler* s;
        bool await_ready() { return s->task_queue.empty(); }

        bool await_suspend(task_base h) {
            printf("next_schedule: queue task %p\n", h.address());
            s->task_queue.emplace_back(std::move(h));

            //printf("next_schedule: queue task size: %ld\n", s->task_queue.size());
            //std::coroutine_handle<> t = s->task_queue.front();
            //s->task_queue.pop();

            // printf("next_schedule: try to switch to %p\n", t.address());
            return true; // go to scheduler
        }
        void await_resume() {
            // printf("next_schedule: await_resume\n");
        }
    } next_schedule;

    scheduler() : next_schedule{this} {}

    void schedule_immediately(task_base h) {
        task_queue.emplace_front(std::move(h));
    }
    // template <typename T>
    void schedule(task_base h) {
        h.promise().self_scheduler = this;
        task_queue.emplace_back(std::move(h));
    }

    void start() {
        while (true) {
            if (task_queue.empty()) {
                break;
            }
            task_base t = task_queue.front();
            task_queue.pop_front();
            printf("scheduler: try to switch to %p\n", t.address());
            printf("scheduler: task status: %d\n", t.promise().status);

            t.resume();
            // printf("scheduler: after resume\n");
            switch(t.promise().status){
                case promise_base::status::suspend:
                    printf("scheduler: task %p suspended\n", t.address());
                    break;
                case promise_base::status::zombie:
                    printf("scheduler: task %p zombie\n", t.address());
                    break;
                case promise_base::status::done:
                    printf("scheduler: task %p done\n", t.address());
                    t.destroy();
                    break;
                case promise_base::status::fail:
                    printf("scheduler: task %p fail\n", t.address());
                    task_base to_be_destroyed = t;
                    while(to_be_destroyed) {
                        task_base caller = to_be_destroyed.promise().caller;

                        if(!caller || caller.promise().fast_fail){
                            printf("scheduler: destroy task %p for its children failed\n", to_be_destroyed.address());
                            to_be_destroyed.destroy();
                            to_be_destroyed = caller;
                        }else{ // caller wants to handle error
                            printf("scheduler: task %p failed, try to resume caller %p\n", to_be_destroyed.address(), caller.address());
                            to_be_destroyed.promise().status = promise_base::status::fail;
                            schedule_immediately(caller);
                            break;
                        }
                        
                    }
                    break;
            }

            

            
        }
    }
};

template <typename return_type>
bool task<return_type>::await_suspend(task_base caller) {

    // this->promise().caller_scheduler =  h.promise().self_scheduler;
    if (!this->promise().self_scheduler) {
        this->promise().self_scheduler = caller.promise().self_scheduler;
    }
    printf("%p: set caller: %p\n", &this->promise(), caller.address());
    this->promise().caller = std::move(caller);
    printf("%p: schedule self immediately\n",&this->promise());
    this->promise().self_scheduler->schedule_immediately(*this);

    // pass control to scheduler
    return true;
}


template <typename return_type>
task<return_type>::opt_type task<return_type>::await_resume() {

    // alloc_failed_task_shortcut
    if (!fail) {
        // cache the result

        
        if(this->promise().status == promise_base::status::zombie) {
            printf("await_resume: promise zombie, destruct: %p\n", &this->promise());
            destroyed = true;
            opt_type result = this->promise().result;
            
            this->destroy();
            return result;
        } else if(this->promise().status == promise_base::status::fail){
            printf("await_resume: promise fail, destruct: %p\n", &this->promise());
            destroyed = true;
            this->destroy();
            return {};
        } else if (this->promise().status == promise_base::status::suspend) {
            printf("await_resume: promise suspend, return result\n");
            return this->promise().result;
        } else {

            printf("assertion failure: status == done but resume\n");

            return {};
        }
        
    } else {
        return {};
    }
}

template <typename return_type>
template <std::convertible_to<return_type> from_t>
std::suspend_always promise<return_type>::yield_value(from_t&& from) {

    this->caller.promise().self_scheduler->schedule_immediately(caller);
    printf("%p: yield value\n", this);
    result = {std::forward<from_t>(from)};  // caching the result in promise
    return {};
}

template <typename return_type>
promise<return_type>::suspend promise<return_type>::final_suspend() noexcept {
    // TODO we may resume caller here
    printf("%p: final suspend\n",this); 
    if (status!=fail && caller) { // waited by some caller
        printf("%p:resume caller: %p\n", this, this->caller.address());
        this->caller.promise().self_scheduler->schedule_immediately(this->caller);
        // wait caller to collect information and destory us
        printf("%p: final suspend return suspend_always\n",this); 
        return {true};
    }else{
        printf("%p: fail/caller not exist, final suspend return suspend_always\n",this); 
        // schedule directly by scheduler, so we just suspend_never
        return {true};
    }
    __builtin_unreachable();

}
 

#endif