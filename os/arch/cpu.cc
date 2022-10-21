#include "cpu.h"

#include <utils/log.h>

#include <cxx/icxxabi.h>
#include <sbi/sbi.h>

#include <mm/allocator.h>

#include <trap/trap.h>
#include <mm/vmem.h>

#include <utils/assert.h>

#include <proc/process.h>


cpu cpus[NCPU];

uint8 __attribute__((aligned(16))) 
__temp_kstack[NCPU][KSTACK_SIZE];

void init_cpus(){
    for (int i = 0; i < NCPU; i++)
    {
        cpus[i].init(i);
    }
}

void cpu::init(int core_id) {
    this->core_id = core_id;
    this->start_cycle = r_time();
    this->temp_kstack = (uint8*)__temp_kstack[core_id];
    __print();
}

task<void> cpu::print(){
    co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n");
    co_await kernel_logger.printf<false>("* print by: %d\n", current_id());
    co_await kernel_logger.printf<false>("* Core ID: %d\n", core_id);
    co_await kernel_logger.printf<false>("* Current Process: %p\n", current_process);
    co_await kernel_logger.printf<false>("* Base Interrupt Status: %d\n", base_interrupt_status);
    co_await kernel_logger.printf<false>("* Number of Off: %d\n", noff);
    co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n\n");
}


void cpu::__print(){
    kernel_console_logger.printf<false>("* ---------- CPU INFO ----------\n");
    kernel_console_logger.printf<false>("* print by: %d\n", current_id());
    kernel_console_logger.printf<false>("* Core ID: %d\n", core_id);
    kernel_console_logger.printf<false>("* Current Process: %p\n", current_process);
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

void cpu::halt(){
    this->halted = true;
    if (core_id == 0) {
        debug_core("waiting for other cores to halt");
        for (int i = 0; i < NCPU; i++) {
            while (!cpus[i].halted);
        }
        __fini_cxx();

        check_memory();
        __infof("[ccore] All finished. Shutdown ...");
        shutdown();
    }
    else{
        while(1){
            asm volatile("wfi");
        }
    }
    
}

void cpu::boot_hart(){
    kvminithart(); // turn on paging
    trap_init_hart();
    plic_init_hart(); // ask PLIC for device interrupts
    booted=true;
}




static void __function_caller(std::function<void()>* func_ptr) {
    (*func_ptr)();
    
    intr_off();
    cpu::my_cpu()->switch_back(nullptr);

    __builtin_unreachable();
}




void cpu::switch_back(context* current) {
    kernel_assert(!intr_get(), "interrupt should be off"); // interrput is off
    int __base_interrupt_status = base_interrupt_status; // save
    swtch(current, &saved_context); // will goto scheduler()
    base_interrupt_status = __base_interrupt_status;
}



void cpu::save_context_and_switch_to(context* to) {
    kernel_assert(!intr_get(), "interrupt should be off"); // interrput is off
    int __base_interrupt_status = base_interrupt_status; // save
    swtch(&saved_context, to); 
    base_interrupt_status = __base_interrupt_status;
}


// save context and switch to another function
void cpu::save_context_and_run(std::function<void()> func) {
    kernel_assert(!intr_get(), "interrupt should be off"); // interrput is off
    int __base_interrupt_status = base_interrupt_status; // save
    
    context temp_context;
    memset(&temp_context, 0, sizeof(context));
    temp_context.sp = (uint64)temp_kstack;
    temp_context.ra = (uint64)__function_caller;
    temp_context.a0 = (uint64)&func;
    swtch(&saved_context, &temp_context);

    base_interrupt_status = __base_interrupt_status;

}