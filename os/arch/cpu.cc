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
    co_await kernel_logger.printf<false>("* Core ID: %d\n", core_id);
    co_await kernel_logger.printf<false>("* Current Thread: %p\n", current_thread);
    co_await kernel_logger.printf<false>("* Base Interrupt Status: %d\n", base_interrupt_status);
    co_await kernel_logger.printf<false>("* Number of Off: %d\n", noff);
    co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n\n");
}
