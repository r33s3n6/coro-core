#ifndef COROUTINE_H
#define COROUTINE_H

#include <coroutine>
#include <optional>
#include <queue>

#include "log/log.h"
#include "mm/allocator.h"
#include "types.h"

struct scheduler;

template <typename T = void>
struct optional_storage;

template <>
struct optional_storage<void> {
    bool has_value = false;
    operator bool() const { return has_value; }
    void set_value(){
        has_value= true;
    }
};



template <typename T>
struct optional_storage : public optional_storage<void> {
    T value;
    explicit operator bool() const { return has_value; }
    operator std::optional<T>() const& {
        if (has_value) {
            return {value};
        } else {
            return std::nullopt;
        }
    }
    operator std::optional<T>() && {
        if (has_value) {
            has_value=false;
            return {std::move(value)};
        } else {
            return std::nullopt;
        }
    }
    template <std::convertible_to<T> from_t>
    void set_value(from_t&& v){
        value = std::forward<T>(v);
        has_value=true;
    }
};

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


struct promise_base;

// task_base is a reference to a coroutine, it will not destruct the coroutine
struct task_base {
    using promise_type = promise_base;
    // take the ownership of the coroutine
    task_base(task_base&& t)
        : _promise(t._promise), _owner(t._owner), alloc_fail(t.alloc_fail) {
        t._promise = nullptr;
        t._owner = false;
    }
    task_base& operator=(task_base&& t) {
        this->_owner = t._owner;
        this->_promise = t._promise;
        t._promise = nullptr;
        t._owner = false;
        return *this;
    }
    // no copy
    task_base(const task_base& t) = delete;
    // for alloc failed
    task_base(const task_fail_t&)
        : _promise(nullptr), _owner(false), alloc_fail(true) {}
    // for container
    task_base() : _promise(nullptr), _owner(false), alloc_fail(false) {}
    // for scheduler and derived class, only there owner can be set to true
    task_base(promise_type* p, bool owner = false)
        : _promise(p), _owner(owner), alloc_fail(false) {}

    // for await_suspend
    template <typename T>
    task_base(std::coroutine_handle<T> h) noexcept
        : _promise(&h.promise()), _owner(false), alloc_fail(false) {
        static_assert(std::is_base_of_v<promise_type, T>);
    }

    explicit operator bool() const { return _promise != nullptr; }

    std::coroutine_handle<promise_type> get_handle() noexcept {
        return std::coroutine_handle<promise_type>::from_promise(*_promise);
    }
    promise_type* get_promise() noexcept { return _promise; }
    void resume() { get_handle().resume(); }

    task_base get_ref() {
        return {_promise, false};
    }

    void clear_owner(){
        _owner = false;
    }
    bool is_owner() const { return _owner; }

   protected:
    promise_type* _promise;
    bool _owner;
    bool alloc_fail;
    

   // private:
    
};

template <typename return_type = void>
struct task;

struct promise_base {

    enum status : uint8_t { suspend, done, fail };
    // our caller, we resume it when we are done
    task_base caller;

    // where we schedule ourselves to
    scheduler* self_scheduler = nullptr;
    bool has_error_handler = false;
    status get_status() const { return _status; }

   protected:
    status _status = suspend;
    //bool has_value = false;
};

template <typename return_type = void>
struct promise : public promise_base {
    optional_storage<return_type> result;

    promise() {}
    ~promise() { 
        // printf("promise %p: destruct\n", this);
         }

    struct yield_awaiter {
        bool await_ready() noexcept { return false; }
        std::coroutine_handle<> await_suspend(task_base t) noexcept {
            // The coroutine is now suspended at the final-suspend point.
            // Lookup its continuation in the promise and resume it.
            promise_base* p = t.get_promise();

            // TODO: task should hide promise

            // we fail
            if(p->get_status() == fail){
                // fast error handling
                task_base curr = t.get_ref();
                // if curr has error handler(its caller), and it has caller,
                // we set it to fail(no need, no value means fail) and resume its caller,
                // if finally we don't have error handler, we destroy the 
                // top coroutine directly and give control back to 
                // scheduler by return std::noop_coroutine
                while (true) {
                    promise_base* curr_promise = curr.get_promise();
                    if (!curr_promise->caller){
                        // top
                        curr.get_handle().destroy();
                        return std::noop_coroutine();
                    }
                    else if (curr_promise->has_error_handler){
                        // curr_promise->_status=fail;
                        return curr_promise->caller.get_handle();
                    }
                    curr = curr_promise->caller.get_ref();
                }

            }
            

            if (p->caller)
                return p->caller.get_handle();
            else {
                
                if(p->get_status() != suspend ){
                    // we are done and we are the top, no one holds
                    // task that owns us(TODO: generator may not)
                    // destroy self and go back to scheduler
                    t.get_handle().destroy();
                }
                
                return std::noop_coroutine();
            }
                
        }
        void await_resume() noexcept {}
    };
    yield_awaiter final_suspend() noexcept { return {}; }

    std::suspend_always initial_suspend() noexcept { return {}; }
    void unhandled_exception() {}

    // task<return_type> get_return_object() { return {this}; }

    promise* get_return_object() { return this; }
    static task_fail_t get_return_object_on_allocation_failure() {
        errorf("alloc task failed\n");
        return task_fail;
    }

    template <std::convertible_to<return_type> from_t, typename T = return_type>
    void return_value(from_t&& from,
                      std::enable_if_t<!std::is_same_v<T, void>, void*> = 0) {
        // result.value = std::forward<return_type>(from);
        result.set_value(std::forward<return_type>(from));
        _status = done;
    }

    // we only allow void task return task_ok
    template <typename T = return_type>
    void return_value(std::enable_if_t<std::is_same_v<T, void>, task_ok_t> t) {
        result.set_value();
        _status = done;
    }
    //template <typename T = return_type>
    //void return_value(
    //    const task_ok_t& t,
    //    std::enable_if_t<std::is_same_v<return_type, void>>* = 0) {
    //    result.set_value();
    //    _status = done;
    //}

    void return_value(const task_fail_t& t) {
        printf("task %p return fail\n", this);
        result.has_value = false;
        _status = fail;
    }

    // we only allow non-void task yield value
    template <std::convertible_to<return_type> from_t, typename T = return_type>
    yield_awaiter yield_value(
        from_t&& from,
        std::enable_if_t<!std::is_same_v<T, void>, void*> = 0) {
        
        // printf("yield value\n");
        result.set_value(std::forward<return_type>(from));
        _status = suspend;

        return {};
    }

    optional_storage<return_type>&& get_value() { return std::move(result); }

    template <typename U>
    U&& await_transform(U&& awaitable) noexcept {
        return static_cast<U&&>(awaitable);
    }
};



template <typename return_type>
struct task : task_base {
    using promise_type = promise<return_type>;
    using handle_type = std::coroutine_handle<promise_type>;
    using opt_type = optional_storage<return_type>;

    task(const task_fail_t& t) : task_base(t) {}
    task() : task_base() {}

    // construct from co_return, we have the ownership of the coroutine
    task(promise_type* p) noexcept : task_base(p, true) {}

   public:
    // task get_ref() {
    //     return {_promise};
    // }
    handle_type get_handle() noexcept {
        return handle_type::from_promise(*(promise_type*)_promise);
    }

    promise_type* get_promise() noexcept {
        return (promise_type*)_promise;
    }

    static handle_type get_handle(promise_base* p) noexcept {
        return handle_type::from_promise(*(promise_type*)p);
    }

    ~task() {
        if (this->is_owner()) {
            // we are done, destroy the coroutine
            // note that task base would not destroy the coroutine
            get_handle().destroy();
        }
    }

    struct task_awaiter {
        // only when we alloc failed, we are ready
        bool await_ready() { return this->alloc_fail; }

        std::coroutine_handle<> await_suspend(task_base caller) {
            if (!p->self_scheduler) {
                p->self_scheduler = caller.get_promise()->self_scheduler;
            }

            p->caller = std::move(caller);

            // resume ourselves, by tail call optimization, we will not create a new
            // stack frame
            return task::get_handle(p);
        }
        // retrieve result generated by co_yield/co_return from promise
        opt_type await_resume() {
            // alloc_failed_task_shortcut
            if (alloc_fail) {
                return {};
            }

            return std::move(p->result);
        }
        
            task_awaiter(promise_base* p, bool alloc_fail) noexcept
            : p((promise_type*)p),alloc_fail(alloc_fail)
        {}
        promise_type* p;
        bool alloc_fail;


    };

    

    task_awaiter operator co_await() noexcept {
        return {_promise, alloc_fail};
    }
    //opt_type&& await_resume() {
    //    // alloc_failed_task_shortcut
    //    if (alloc_fail) {
    //        return null_storage;
    //    }
//
    //    return std::move(this->_promise->result);
    //}
};
/*
// for generator
template <typename return_type>
struct gen_task : task_base {
    using promise_type = promise<return_type>;
    using handle_type = std::coroutine_handle<promise_type>;
    using opt_type = optional_storage<return_type>;

    gen_task(const task_fail_t& t) : task_base(t) {}
    gen_task(const gen_task& gt): task_base(gt._promise,false){}
    gen_task() : task_base() {}

    // construct from co_return, we have the ownership of the coroutine
    gen_task(promise_type* p) noexcept : task_base(p, true) {}

   public:

    handle_type get_handle() noexcept {
        return handle_type::from_promise(*(promise_type*)_promise);
    }

    promise_type* get_promise() noexcept {
        return (promise_type*)_promise;
    }

    static handle_type get_handle(promise_base* p) noexcept {
        return handle_type::from_promise(*(promise_type*)p);
    }

    ~gen_task() {
       printf("gentask destruct\n");
    }

    bool await_ready() { return false; }

    bool await_suspend(task_base caller) {
        printf("gen_task:await_suspend\n");
        get_handle().resume();
        printf("gen_task:await_suspend: resume end\n");
        
        return false;
    }
    // retrieve result generated by co_yield/co_return from promise
    opt_type await_resume() {
        if(!get_promise()->result){
            printf("shit\n");
        }
        return std::move(get_promise()->result);
    }

    


};*/

// TODO fix ownership problem
struct scheduler {
    std::deque<task_base> task_queue;

    // TODO: may schedule to different scheduler
    //struct next_schedule_t {
    //    scheduler* s;
    //    bool await_ready() { return s->task_queue.empty(); }
    //
    //    bool await_suspend(task_base h) {
    //        printf("next_schedule: queue task %p\n", h.address());
    //        s->task_queue.emplace_back(std::move(h));
    //
    //        return true;  // go to scheduler
    //    }
    //    void await_resume() {}
    //} next_schedule;

    // scheduler() : next_schedule{this} {}

    // void schedule_immediately(task_base h) {
    //     task_queue.emplace_front(std::move(h));
    // }
    // template <typename T>
    void schedule(task_base&& h) {
        // let them clear themselves on final_suspend
        if(!h){
            printf("scheduler: empty task\n");
            return;
        }

        if(!h.get_promise()->caller){
            h.clear_owner();
        }
       
        h.get_promise()->self_scheduler = this;
        task_queue.emplace_back(std::move(h));
    }

    bool free() { return task_queue.empty(); }

    void start() {
        while (true) {
            if (task_queue.empty()) {
                break;
            }
            // all tasks we own, we start it here
            task_base t = std::move(task_queue.front());
            task_queue.pop_front();
            printf("scheduler: try to switch to %p\n", t.get_promise());
            printf("scheduler: task status: %d\n", t.get_promise()->get_status());

            t.resume();
            // we do not track the status of tasks

        }
    }
};

struct this_scheduler_t {
    enum class _Construct { _Token };
    explicit constexpr this_scheduler_t(_Construct) {}
    bool await_ready() const { return false; }
    std::coroutine_handle<> await_suspend(task_base h) const {
        scheduler* s = h.get_promise()->self_scheduler;
        if (s->free()) {
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
/*
struct get_taskbase_t {
    bool await_ready() { return false; }

    task_base _task;
    bool await_suspend(task_base h) {
        _task = h;
        return false;
    }
    task_base await_resume() {
        return _task;
    }
};*/

#endif