#ifndef ATOMIC_MUTEX_H
#define ATOMIC_MUTEX_H

#include "spinlock.h"

#include <utils/wait_queue.h>
#include <coroutine.h>

// this may cause hungry
class coro_mutex {

    public:
    coro_mutex(const char* name = "unnamed") : _guard_lock(name) {}

    task<void> lock();
    void unlock();

    private:
    bool _locked = false;
    spinlock _guard_lock;
    wait_queue _wait_queue;

};



#endif // MUTEX_H
