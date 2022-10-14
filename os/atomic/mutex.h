#ifndef ATOMIC_MUTEX_H
#define ATOMIC_MUTEX_H

#include "spinlock.h"

class mutex {

    
    mutex(const char* name = "unnamed") : _guard_lock(name) {}

    private:
    uint32 _locked = false; // Is the lock held?
    spinlock _guard_lock;
    int _tid = 0; // The thread holding the lock.
    // const char *_name;
};

void init_mutex(struct mutex *mutex);
void acquire_mutex_sleep(struct mutex *mu);
void release_mutex_sleep(struct mutex *mu);
int holdingsleep(struct mutex *lk);

#endif // MUTEX_H
