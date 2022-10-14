#include "mutex.h"
#include <arch/riscv.h>
#include <proc/proc.h>
#include <ccore/types.h>


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