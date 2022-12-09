#include "wait_queue.h"

#include <arch/cpu.h>

void wait_queue_base::wait_done(sleepable* s, spinlock& lock) {
    this->sleep(s);
    lock.unlock();
    cpu::my_cpu()->yield();
    lock.lock();
    
}