#include "mutex.h"
#include <arch/riscv.h>
#include <proc/process.h>
#include <ccore/types.h>

#include <utils/assert.h>

task<void> coro_mutex::lock() {
    _guard_lock.lock();
    while (_locked) {
        co_await _wait_queue.done(_guard_lock);
    }
    _locked = true;
    _guard_lock.unlock();
    co_return task_ok;
}

void coro_mutex::unlock() {
    _guard_lock.lock();
    _locked = false;
    _wait_queue.wake_up_one();
    _guard_lock.unlock();
}


