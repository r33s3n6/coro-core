#ifndef ATMOIC_SPINLOCK_H
#define ATMOIC_SPINLOCK_H

#include <ccore/types.h>

#include <utils/printf.h>
#include <utils/panic.h>


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
    void lock() {
        cpu* current_cpu = cpu::my_cpu();
        current_cpu->push_off(); // disable interrupts to avoid deadlock.

        if (holding()) {
            __printf("lock \"%s\" is held by core %d, cannot be reacquired", _name, _cpu->get_core_id());
            panic("This cpu is acquiring a acquired lock");
        }

        #ifdef TIMEOUT
        uint64 start = r_cycle();
        #endif

        while (__sync_lock_test_and_set(&_locked, 1) != 0)
            {
        #ifdef TIMEOUT
                uint64 now = r_cycle();
                if(now-start > SECOND_TO_CYCLE(10)){
                    __printf("timeout lock name: %s, hold by cpu %d", _name, _cpu->get_core_id());
                    panic("spinlock timeout");
                }
        #endif
            }

        // Tell the C compiler and the processor to not move loads or stores
        // past this point, to ensure that the critical section's memory
        // references happen strictly after the lock is acquired.
        // On RISC-V, this emits a fence instruction.
        __sync_synchronize();

        _cpu = current_cpu;
    }

    // Release the lock.
    void unlock() {
        // KERNEL_ASSERT(holding(slock), "a core should hold the lock if it wants to release it");
        if (!holding()) {
            __printf("Error release lock: %s\n", _name);
            panic("Try to release a lock when not holding it");
        }

        _cpu = nullptr;

        // Tell the C compiler and the CPU to not move loads or stores
        // past this point, to ensure that all the stores in the critical
        // section are visible to other CPUs before the lock is released,
        // and that loads in the critical section occur strictly before
        // the lock is released.
        // On RISC-V, this emits a fence instruction.
        __sync_synchronize();

        // Release the lock, equivalent to lk->locked = 0.
        // This code doesn't use a C assignment, since the C standard
        // implies that an assignment might be implemented with
        // multiple store instructions.
        // On RISC-V, sync_lock_release turns into an atomic swap:
        //   s1 = &lk->locked
        //   amoswap.w zero, zero, (s1)
        __sync_lock_release(&_locked);

        cpu::my_cpu()->pop_off();
    }

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
