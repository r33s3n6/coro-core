#ifndef ATOMIC_LOCK_H
#define ATOMIC_LOCK_H

// #include "mutex.h"
#include "spinlock.h"

template <typename lock_type>
class lock_guard {
public:
    lock_guard(lock_type &lock) : _lock(lock) {
        _lock.lock();
    }
    ~lock_guard() {
        _lock.unlock();
    }
private:
    lock_type& _lock;
};

template< typename lock_type >
lock_guard<lock_type> make_lock_guard(lock_type& lock) {
    return lock_guard<lock_type>(lock);
}


#endif // LOCK_H
