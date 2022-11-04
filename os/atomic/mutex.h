#ifndef ATOMIC_MUTEX_H
#define ATOMIC_MUTEX_H

#include "spinlock.h"
#include <utils/wait_queue.h>

class mutex {

    
    mutex(const char* name = "unnamed") : _guard_lock(name) {}

    void lock(sleepable* sleeper);
    void unlock();

    private:
    uint32 _locked = false; // Is the lock held?
    spinlock _guard_lock;
    int _pid = 0; // The process holding the lock.
    wait_queue _wait_queue;

};


void acquire_mutex_sleep(struct mutex *mu);
void release_mutex_sleep(struct mutex *mu);
int holdingsleep(struct mutex *lk);

#endif // MUTEX_H
