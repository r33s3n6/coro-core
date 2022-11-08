#ifndef ATOMIC_LOCK_H
#define ATOMIC_LOCK_H


#include "spinlock.h"
// #include "mutex.h"

void __debug_enable();
void __debug_disable();
void __debug(void* addr);


template <typename lock_type>
class lock_guard {
public:
    lock_guard(lock_type &lock): _lock(lock) {
        __debug(&lock);
        _lock.lock();
    }
    ~lock_guard() {
        _lock.unlock();
    }
private:
    lock_type& _lock;
};

template <typename lock_type>
inline lock_guard<lock_type> make_lock_guard(lock_type& lock) {
    return lock_guard<lock_type>(lock);
}


#endif // LOCK_H
