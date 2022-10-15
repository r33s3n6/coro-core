#include "cpu.h"

#include <utils/log.h>


cpu cpus[NCPU];

void init_cpus(){
    for (int i = 0; i < NCPU; i++)
    {
        cpus[i].init(i);
    }
}

task<void> cpu::print(){
    co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n");
    co_await kernel_logger.printf<false>("* print by: %d\n", current_id());
    co_await kernel_logger.printf<false>("* Core ID: %d\n", core_id);
    co_await kernel_logger.printf<false>("* Current Thread: %p\n", current_thread);
    co_await kernel_logger.printf<false>("* Base Interrupt Status: %d\n", base_interrupt_status);
    co_await kernel_logger.printf<false>("* Number of Off: %d\n", noff);
    co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n\n");
}


void cpu::__print(){
    kernel_console_logger.printf<false>("* ---------- CPU INFO ----------\n");
    kernel_console_logger.printf<false>("* print by: %d\n", current_id());
    kernel_console_logger.printf<false>("* Core ID: %d\n", core_id);
    kernel_console_logger.printf<false>("* Current Thread: %p\n", current_thread);
    kernel_console_logger.printf<false>("* Base Interrupt Status: %d\n", base_interrupt_status);
    kernel_console_logger.printf<false>("* Number of Off: %d\n", noff);
    kernel_console_logger.printf<false>("* ---------- CPU INFO ----------\n\n");
}

// push_off/pop_off are like intr_off()/intr_on() except that they are matched:
// it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
// are initially off, then push_off, pop_off leaves them off.
void cpu::pop_off(){
    if (intr_get())
        panic("pop_off - interruptible");
    if (noff < 1) {
        // __print();
        panic("pop_off");
    }
        
    noff -= 1;
    if (noff == 0 && base_interrupt_status) {
        intr_on();
    }
}

void cpu::push_off(){
    int old = intr_get();

    intr_off();
    if (noff == 0) {
        base_interrupt_status = old;
    }
    noff += 1;
}

