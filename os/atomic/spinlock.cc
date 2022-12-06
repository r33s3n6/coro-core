#include "spinlock.h"

#include <arch/cpu.h>

#include <utils/panic.h>
#include <utils/log.h>

// #define SPINLOCK_TIMEOUT_CHECK

// Acquire the lock.
// Loops (spins) until the lock is acquired.
void spinlock::lock() {
    // turn off the interrupt and get the current cpu id
    int old = cpu::local_irq_save();
    int id = cpu::current_id();

    if (__holding()) {
        kernel_console_logger.printf<false>(
            logger::log_level::ERROR, 
            "lock \"%s\" is held by core %d, cannot be reacquired",
            _name, core_id);
        __panic("This cpu is acquiring a acquired lock");
    }

    #ifdef SPINLOCK_TIMEOUT_CHECK
    uint64 start = r_cycle();
    #endif

    while (__sync_lock_test_and_set(&_locked, 1) != 0)
        {
    #ifdef SPINLOCK_TIMEOUT_CHECK
            uint64 now = r_cycle();
            if(now-start > timer::SECOND_TO_CYCLE(3)) {
                __errorf("timeout lock name: %s, hold by cpu %d", _name, id);
                __panic("spinlock timeout");
            }
    #endif
        }

    // Tell the C compiler and the processor to not move loads or stores
    // past this point, to ensure that the critical section's memory
    // references happen strictly after the lock is acquired.
    // On RISC-V, this emits a fence instruction.
    __sync_synchronize();

    core_id = id;
    old_status = old;

    //kernel_console_logger.printf<false>(
    //            logger::log_level::WARN, 
    //            "acquire lock: %s\n", _name);
}

// Release the lock.
void spinlock::unlock() {
    // kernel_assert(holding(slock), "a core should hold the lock if it wants to release it");
    if (!__holding()) {
        kernel_console_logger.printf<false>(
                    logger::log_level::ERROR, 
                    "Error release lock: %s\n", _name);
        __panic("Try to release a lock when not holding it");
    }


    core_id = -1;
    int old = old_status;
    old_status = 0;


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

    // restore the interrupt status
    cpu::local_irq_restore(old);
}

bool spinlock::__holding() {
    return (_locked && core_id == cpu::current_id());
}


bool spinlock::holding() {
    int old = cpu::local_irq_save();
    bool ret = __holding();
    cpu::local_irq_restore(old);
    return ret;
}