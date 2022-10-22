#ifndef UTILS_SHARED_PTR_H
#define UTILS_SHARED_PTR_H

#include <atomic/lock.h>

struct ref_count_t {
    uint32 count;
    spinlock lock{"shared_ptr.lock"};
    ref_count_t(int init) : count(init) {}
    void inc() {
        lock.lock();
        count++;
        lock.unlock();
    }
    uint32 dec() {
        lock.lock();
        int temp = --count;
        lock.unlock();
        return temp;
    }
};

template <typename T>
class shared_ptr {
   private:
    T* ptr;
    ref_count_t* ref_count;

   public:
    shared_ptr() : ptr(nullptr), ref_count(nullptr) {}

    shared_ptr(T* ptr) : ptr(ptr), ref_count(new ref_count_t(1)) {}

    template <typename other_type>
    friend class shared_ptr;

    template <typename other_type,
              typename = typename std::enable_if<
                  std::is_convertible<other_type*, T*>::value>::type>
    shared_ptr(const shared_ptr<other_type>& other) noexcept

        : ptr(other.ptr), ref_count(other.ref_count) {
        if (ptr) {
            ref_count->inc();
        }
    }

    template <typename other_type>
    shared_ptr& operator=(const shared_ptr<other_type> other) noexcept {
        if (ptr) {
            if (ref_count->dec() == 0) {
                delete ptr;
                delete ref_count;
            }
        }

        ptr = other.ptr;
        ref_count = other.ref_count;

        if (ptr) {
            ref_count->inc();
        }
        return *this;
    }

    ~shared_ptr() {
        if (ptr) {
            if (ref_count->dec() == 0) {
                delete ptr;
                delete ref_count;
            }
        }
    }

    T* operator->() { return ptr; }

    T& operator*() { return *ptr; }

    bool operator==(const shared_ptr& other) { return ptr == other.ptr; }

    bool operator!=(const shared_ptr& other) { return ptr != other.ptr; }

    bool operator!() { return !ptr; }

    void reset(T* ptr) {
        if (this->ptr) {
            if (ref_count->dec() == 0) {
                delete this->ptr;
                delete ref_count;
            }
        }

        this->ptr = ptr;

        if (ptr) {
            ref_count = new ref_count_t(1);
        } else {
            ref_count = nullptr;
        }
    }

    T* get() { return ptr; }
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args... args) {
    return shared_ptr<T>(new T(args...));
}

#endif