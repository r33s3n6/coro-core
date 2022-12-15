#ifndef ATMOIC_SPINLOCK_H
#define ATMOIC_SPINLOCK_H

#include <ccore/types.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

// if you get a deadlock in debugging, define TIMEOUT so that spinlock will panic after
// some time, and you will see the debugging information
// but this will impact the performance greatly 

// #define TIMEOUT
class cpu;

class spinlock {
public:
    spinlock(const char* name = "unnamed") :
    _locked(0) 
    ,core_id(-1) 
    #ifdef LOCK_DEBUG
    ,_name(name) 
    #endif
    {}

    ~spinlock() {}

    // Acquire the lock.
    // Loops (spins) until the lock is acquired.
    void lock();

    // Release the lock.
    void unlock();

    // Check whether this cpu is holding the lock.
    // Interrupts must be off.
    bool __holding();

    bool holding();


    private:
    volatile uint8 _locked;
    uint8 old_status;
    uint16 core_id;
    
    #ifdef LOCK_DEBUG
    const char* _name;
    #endif
};


#pragma GCC diagnostic pop
#endif // SPINLOCK_H
