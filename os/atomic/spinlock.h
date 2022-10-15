#ifndef ATMOIC_SPINLOCK_H
#define ATMOIC_SPINLOCK_H

#include <ccore/types.h>

#include <arch/cpu.h>

// if you get a deadlock in debugging, define TIMEOUT so that spinlock will panic after
// some time, and you will see the debugging information
// but this will impact the performance greatly 

// #define TIMEOUT


class spinlock {
public:
    spinlock(const char* name = "unnamed") :_locked(0), _cpu(0), _name(name) {}

    ~spinlock() {}

    // Acquire the lock.
    // Loops (spins) until the lock is acquired.
    void lock();

    // Release the lock.
    void unlock();

    // Check whether this cpu is holding the lock.
    // Interrupts must be off.
    int holding() {
        return (_locked && _cpu == cpu::my_cpu());
    }


    private:
    volatile uint32 _locked;
    cpu* _cpu;
    const char* _name;
};



#endif // SPINLOCK_H
