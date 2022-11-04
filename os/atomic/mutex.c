#include "mutex.h"
#include <arch/riscv.h>
#include <proc/process.h>
#include <ccore/types.h>

#include <utils/assert.h>

void mutex::lock(sleepable* sleeper) {
    // process* p = cpu::my_cpu()->get_current_process();
    // kernel_assert(p != nullptr, "lock() called from interrupt context");
    _guard_lock.lock();
    while (_locked) {
        _guard_lock.unlock();
        _wait_queue.sleep(sleeper);
        _guard_lock.lock();
    }
}


void acquire_mutex_sleep(struct mutex *mu) {
    acquire(&mu->guard_lock);
    while (mu->locked) {
        debugcore("acquire mutex sleep start");
        sleep(mu, &mu->guard_lock);
        debugcore("acquire mutex sleep end");
    }
    mu->locked = 1;
    mu->pid = curr_proc()->pid;
    release(&mu->guard_lock);
}
void release_mutex_sleep(struct mutex *mu) {
    acquire(&mu->guard_lock);
    mu->locked = 0;
    mu->pid = 0;
    wakeup(mu);
    release(&mu->guard_lock);
}

int holdingsleep(struct mutex *lk) {
    int ret;

    acquire(&lk->guard_lock);
    ret = lk->locked && (lk->pid == curr_proc()->pid);
    release(&lk->guard_lock);
    return ret;
}