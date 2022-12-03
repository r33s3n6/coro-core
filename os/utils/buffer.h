#ifndef UTILS_BUFFER_H
#define UTILS_BUFFER_H

#include <atomic/lock.h>
#include <atomic/mutex.h>

#include <utils/panic.h>
#include <utils/utility.h>

#include <coroutine.h>

#include <concepts>


template <typename ref_type>
concept referenceable_type = 
requires(ref_type r) {
    requires true;
    // { r.get() } -> std::same_as<task<void>>;
    // { r.put() } -> std::same_as<void>;
    // { r.mark_dirty() } -> std::same_as<void>;
};

template <typename buf_type>
concept bufferable_type = 
requires(buf_type b) {
    { b.load() } -> std::same_as<task<void>>;
    { b.flush() } -> std::same_as<task<void>>;
};

template <referenceable_type ref_type>
class reference_guard : noncopyable {

    private:
    
    ref_type* ptr = nullptr;
    bool hold = false;

    public:
    constexpr reference_guard() {}
    constexpr reference_guard(ref_type* ptr) : ptr(ptr), hold(true) {}
    constexpr reference_guard(reference_guard&& other) : ptr(other.ptr), hold(other.hold) {
        other.hold = false;
    }
    template<referenceable_type T>
    constexpr reference_guard(reference_guard<T>&& other) : ptr((ref_type*)(other.ptr)), hold(other.hold) {
        other.hold = false;
    }
    


    constexpr reference_guard& operator=(reference_guard&& other) {
        std::swap(ptr, other.ptr);
        std::swap(hold, other.hold);
        return *this;
    }

    ~reference_guard() {
        if(hold) {
            ptr->put();
        }
    }

    public:
    task<const ref_type*> get() {
        co_await __get();
        co_return ptr;
    }


    ref_type* operator->() {
        return ptr;
    }

    ref_type& operator*() {
        return *ptr;
    }

    void put() {
        if(!hold) {
            panic("reference_guard::put() called twice");
        }
        hold = false;
        ptr->put();
    }

    private:
    task<void> __get() {
        if(!hold) {
            co_await ptr->get();
            hold = true;
        }
        co_return task_ok;

    }

    template <referenceable_type T>
    friend class reference_guard;

};

template <typename derived_type>
class referenceable_buffer : noncopyable {
    
public:
    referenceable_buffer()  {}
    ~referenceable_buffer() {}



    derived_type& __get_derived() {
        return *static_cast<derived_type*>(this);
    }
    
    task<void> load() {
        if(!_valid) {
            co_await __get_derived().__load();
            _valid = true;
            _dirty = false;
        }
        co_return task_ok;
    }

    task<void> flush() {
        if (_valid && _dirty) {
            co_await __get_derived().__flush();
            _dirty = false;
        }
        co_return task_ok;
    }

    



    task<void> get() {
        co_await _in_use_mutex.lock();
        co_await load();
        co_return task_ok;
    }

    void put() {
        _in_use_mutex.unlock();
    }

    void mark_dirty() {
        _dirty = true;
    }

    void mark_clean() {
        _dirty = false;
    }

    bool is_dirty() const {
        return _dirty;
    }

    bool flush_needed() const {
        return _valid && _dirty;
    }

    void mark_invalid() {
        _valid = false;
    }

    void mark_valid() {
        _valid = true;
    }

    bool is_valid() const {
        return _valid;
    }

    task<reference_guard<derived_type>> get_ref() {
        co_await get();
        co_return reference_guard<derived_type>{&__get_derived()};
    }
    

    private:

    coro_mutex _in_use_mutex {"bufferable.in_use_mutex"};
    
    bool _valid = false;
    bool _dirty = false;


};






#endif