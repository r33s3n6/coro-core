#ifndef COROUTINE_H
#define COROUTINE_H

#include <coroutine>
#include <optional>
#include <queue>

// #include <mm/allocator.h>
#include <ccore/types.h>

#include <utils/printf.h>
#include <utils/panic.h>
#include <utils/utility.h>

#include <utils/sleepable.h>
#include <utils/list.h>

#include <arch/config.h>
#include <atomic/lock.h>

struct task_scheduler;

template <typename T = void>
struct optional_storage;

template <>
struct optional_storage<void> {
    bool has_value = false;
    operator bool() const { return has_value; }
    void set_value(){
        has_value = true;
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
        value = std::forward<from_t>(v);
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

// task destruct the promise if it is the owner
// task_base will never destruct promise
// destruct function is defined in the derived class
struct task_base : noncopyable, sleepable {
    using promise_type = promise_base;
    // take the ownership of the coroutine
    task_base(task_base&& t);
    task_base& operator=(task_base&& t);

    // for alloc failed
    task_base(const task_fail_t&)
        : _promise(nullptr), alloc_fail(true), _owner(false) {}
    
    // for container
    task_base() : _promise(nullptr), alloc_fail(false), _owner(false) {}

    // for scheduler and derived class
    task_base(promise_type* p, bool owner = false);

    // for await_suspend
    template <typename T>
    task_base(std::coroutine_handle<T> h) noexcept
        : _promise(&h.promise()), alloc_fail(false), _owner(false) {
        static_assert(std::is_base_of_v<promise_type, T>);
    }

    explicit operator bool() const { return _promise != nullptr; }

    std::coroutine_handle<promise_type> get_handle() noexcept {
        return std::coroutine_handle<promise_type>::from_promise(*_promise);
    }

    promise_type* get_promise() noexcept { return _promise; }

    void resume();

    task_base get_ref() {
        return {_promise, false};
    }

    void clear_owner() {
        _owner = false; 
    }

    void sleep() {
        // do nothing
    }

    void wake_up();

   protected:
    promise_type* _promise;
    bool alloc_fail;
    bool _owner;
    
};

template <typename return_type = void>
struct task;

struct promise_base {

    ~promise_base();

    enum status : uint8_t { init, running, suspend, done, fail };
    // our caller, we resume it when we are done
    task_base caller {};

    // where we schedule ourselves to
    task_scheduler* self_scheduler = nullptr;

    // if our caller can handle error, if so,
    // when we or our callee fail, we resume our caller
    bool has_error_handler = false;
    
    // do not pass control to scheduler
    bool no_yield = false;

    // track the ownership of the coroutine
    // task_base* owned_by = nullptr;

    status get_status() const { return _status; }
    void set_fail() { _status = fail; }
    void set_running() { _status = running; }

#ifdef HANDLE_MEMORY_ALLOC_FAIL
    static task_fail_t get_return_object_on_allocation_failure();
#endif

   protected:
   friend class task_base;

    status _status = init;
};




template <typename return_type = void>
struct promise : public promise_base {
    optional_storage<return_type> result;

    promise() {}
    ~promise() { 
        // printf("promise %p: destruct\n", this);
    }

    // awaiter that we yield some value and pass control to our caller
    struct yield_awaiter {
        bool await_ready() noexcept { return false; }


        // The coroutine is now suspended at a suspend point.
        // t is original coroutine, waiting for us to yield value
        // and continue the caller
        std::coroutine_handle<> await_suspend(task_base t) noexcept {
            promise_base* p = t.get_promise();
            // printf("yield_awaiter: await_suspend: %p\n", p);

            // we fail
            if(p->get_status() == fail){
                // fast error handling
                task_base curr = t.get_ref();
                // if curr has error handler(its caller), we resume its caller
                // otherwise, we set curr = curr.caller and continue this process
                // eventually we would encounter task whose caller is task_executor,
                // which has no caller and set the task's have_error_handler to true
                // so we will resume the task_executor. if not so, we panic

                while (true) {
                    promise_base* curr_promise = curr.get_promise();
                    if (!curr_promise->caller){
                        // top
                        panic("no error handler found");
                    }
                    else if (curr_promise->has_error_handler){
                        // we set status of current task to fail,
                        // and resume its caller, so the caller would
                        // handle error properly
                        curr_promise->set_fail();
                        return curr_promise->caller.get_handle();
                    }
                    curr = curr_promise->caller.get_ref();
                }

            }
            
            // if we have caller, we resume our caller, 
            // our caller will destroy us after consume value
            if (p->caller) {
                //__printf("yield_awaiter: await_suspend: resume caller: %p\n", p->caller.get_promise());
                return p->caller.get_handle();
            }  
            else {
                if( p->get_status() != suspend){
                    // we are done and we have no caller, so we
                    // destroy ourself and go back to scheduler
                    //__printf("DEBUG: destroy promise :%p\n", t.get_promise());
                    
                    t.get_handle().destroy();
                }
                // back to scheduler
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


    template <std::convertible_to<return_type> from_t, typename T = return_type>
    void return_value(from_t&& from,
                      std::enable_if_t<!std::is_same_v<T, void>, void*> = 0) {
        // result.value = std::forward<return_type>(from);
        result.set_value(std::forward<from_t>(from));
        _status = done;
    }

    // we only allow void task return task_ok
    template <typename T = return_type>
    void return_value(std::enable_if_t<std::is_same_v<T, void>, task_ok_t>) {
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

    void return_value(const task_fail_t&) {
        // printf("task %p return fail\n", this);
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
    // for compiler to find out promise type
    using promise_type = promise<return_type>;

    using handle_type = std::coroutine_handle<promise_type>;
    using opt_type = optional_storage<return_type>;
    using ret_opt_type = std::conditional_t<std::is_same_v<return_type, void>, bool , std::optional<return_type>>;

    task(const task_fail_t& t) : task_base(t) {}
    task() : task_base() {}

    // construct from compiler, we have the ownership of the coroutine
    task(promise_type* p) noexcept : task_base(p, true) {
        //printf("task: construct: %p\n", p);
    }

    task(task&& t) noexcept : task_base(std::move(t)) {
        //__printf("task: move construct: %p\n", t.get_promise());
    }
    task& operator=(task&& t) noexcept {
        task_base::operator=(std::move(t));

        //__printf("task: move construct: %p\n", t.get_promise());
        return *this;
    }


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
        if (this->_owner) {
            // we are done, destroy the coroutine

            // __printf("~task: destroy: %p\n", _promise);
            get_handle().destroy();
        }
    }

    struct task_awaiter {
        // only when we alloc failed, we are ready
        bool await_ready() { return this->alloc_fail; }

        std::coroutine_handle<> await_suspend(task_base caller) {
            promise_base* caller_promise = caller.get_promise();
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
        // retrieve result generated by co_yield/co_return from promise
        ret_opt_type await_resume() {

            // alloc_failed_task_shortcut
            if (alloc_fail) {
                return {};
            }

            if (p->get_status() == promise_base::fail) {
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

task<void> __task_executor(promise<void>* p);

class task_queue {
    list<task_base> queue;
    spinlock lock {"task_queue.lock"};

public:
    task_base pop();
    void push(task_base&& proc);
    bool empty() { 
        return queue.empty();
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
            h = __task_executor((promise<void>*)buf.get_promise());
        }

        __schedule(std::move(h));

    }

    void __schedule(task_base&& h) {
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