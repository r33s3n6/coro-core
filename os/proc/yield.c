#include <proc/proc.h>

// Give up the CPU for one scheduling round.
void yield(void) {
    pushtrace(0x3005);
    struct proc *p = curr_proc();
    kernel_assert(p != nullptr, "yield() has no current proc");
    acquire(&p->lock);
    pushtrace(0x3035);
    p->state = RUNNABLE;
    switch_to_scheduler();
    pushtrace(0x3030);
    release(&p->lock);
}
