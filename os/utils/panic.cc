#include <ccore/types.h>
#include <sbi/sbi.h>
#include <arch/config.h>
#include <arch/riscv.h>

#include <arch/cpu.h>

#include <utils/log.h>

#include "printf.h"

uint32 panic_pending[NCPU] {0};

void __trace_panic() {

}

void __panic(const char* s) {
    __trace_panic();
    __printf("panic: %s\n", s);
    shutdown();
    __builtin_unreachable();
}

void _panic(const char *s, const char* file, int line)
{
    __trace_panic();
    
    auto cpu_ref = cpu::my_cpu();
    int cpu_id = cpu_ref->get_core_id();
    if (panic_pending[cpu_id]) {
        __printf("during panic backtrace, another panic: %s\n", s);
    } else {
        panic_pending[r_tp()] = 1;
        kernel_console_logger.printf<true>(logger::log_level::ERROR, "[%d] %s:%d: panic: %s\n", cpu_id, file, line, s);
        cpu_ref->backtrace_coroutine();
        
    }
    
    shutdown();
    __builtin_unreachable();

}

extern "C" void abort() {
    panic("std library: abort");
    __builtin_unreachable();
}