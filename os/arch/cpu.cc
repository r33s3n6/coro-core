#include "cpu.h"

#include <utils/log.h>

#include <cxx/icxxabi.h>
#include <sbi/sbi.h>

#include <mm/allocator.h>

#include <mm/vmem.h>
#include <trap/trap.h>

#include <utils/assert.h>
#include <utils/backtrace.h>
#include <utils/wait_queue.h>

#include <proc/process.h>

#include <task_scheduler.h>
#include <proc/scheduler.h>


cpu cpus[NCPU];

uint8 __attribute__((aligned(16))) __temp_kstack[NCPU][KSTACK_SIZE];

void context::print() {
    debug_core(
        "context: ra: %p, sp: %p, s0: %p, s1: %p, s2: %p, s3: %p, s4: %p, s5: "
        "%p, s6: %p, s7: %p, s8: %p, s9: %p, s10: %p, s11: %p, a0: %p",
        ra, sp, s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, a0);
}

void init_cpus() {
    for (int i = 0; i < NCPU; i++) {
        cpus[i].init(i);
    }
}

void cpu::init(int core_id) {
    this->core_id = core_id;
    this->start_cycle = r_time();
    this->temp_kstack = (uint8*)__temp_kstack[core_id] + KSTACK_SIZE;
    __print();
}

// task<void> cpu::print() {
//     co_await kernel_logger.printf<false>("* ---------- CPU INFO ----------\n");
//     co_await kernel_logger.printf<false>("* print by: %d\n", current_id());
//     co_await kernel_logger.printf<false>("* Core ID: %d\n", core_id);
//     co_await kernel_logger.printf<false>("* Current Process: %p\n",
//                                          current_process);
//     co_await kernel_logger.printf<false>(
//         "* ---------- CPU INFO ----------\n\n");
// }

void cpu::__print() {
    kernel_console_logger.printf<false>("* ---------- CPU INFO ----------\n");
    kernel_console_logger.printf<false>("* print by: %d\n", current_id());
    kernel_console_logger.printf<false>("* Core ID: %d\n", core_id);
    kernel_console_logger.printf<false>("* Current Process: %p\n",
                                        current_process);
    kernel_console_logger.printf<false>("* Temp Kernel Stack: %p\n",
                                        temp_kstack);
    kernel_console_logger.printf<false>("* ---------- CPU INFO ----------\n\n");
}

cpu_ref cpu::my_cpu() {
    return {cpu_ref::_Construct::_Token};
}

void cpu::halt() {
    this->halted = true;
    if (core_id == 0) {
        debug_core("waiting for other cores to halt");
        for (int i = 0; i < NCPU; i++) {
            while (!cpus[i].halted)
                ;
        }
        __fini_cxx();

        // check_memory();
        __infof("[ccore] All finished. Shutdown ...");
        shutdown();
    } else {
        while (1) {
            asm volatile("wfi");
        }
    }
}

void cpu::boot_hart() {
    kvminithart();  // turn on paging
    trap_init_hart();
    plic_init_hart();  // ask PLIC for device interrupts
    booted = true;
}

void cpu::sample(uint64 all, uint64 busy) {
    // record one sample
    
    debug_core("sample: (%l/1000) %d %d", busy*1000/all, kernel_task_queue.size(), kernel_process_queue.size());
    sample_duration[next_slot] = all;
    busy_time[next_slot] = busy;
    next_slot = (next_slot + 1) % SAMPLE_SLOT_COUNT;
}

static void __function_caller(std::function<void()>* func_ptr) {
    kernel_assert(!cpu::local_irq_on(), "local_irq should be disabled");
    func_ptr->operator()();

    cpu::__my_cpu()->switch_back(nullptr);

    __builtin_unreachable();
}

void cpu::yield() {
    // there's no need to save interrupt state,
    // because on next run, it will be set properly by scheduler
    kernel_assert(!cpu::local_irq_on(), "local_irq should be disabled when yield");

    //void* ra0 = __builtin_return_address(0);
    //debug_core("yield: ra[0]: %p", ra0);

    switch_back(get_kernel_process()->get_context());
}

void cpu::switch_back(context* current) {
    kernel_assert(
        !cpu::local_irq_on() || (cpu::local_irq_on() && !(r_sie() & SIE_STIE)),
        "timer interrupt should be off");  // interrupt is off



    // void* ra0 = __builtin_return_address(0);
// 
    // debug_core("switch back: ra[0]: %p", ra0);
    // saved_context.print();
    swtch(current, &saved_context);  // will goto scheduler()

    // debug_core("switch_back: back");
    // current->print();
}

void cpu::save_context_and_switch_to(context* to) {
    kernel_assert(
        !cpu::local_irq_on() || (cpu::local_irq_on() && !(r_sie() & SIE_STIE)),
        "timer interrupt should be off");  // interrupt is off

    // debugf("save_context_and_switch_to: %p", to);
    // debugf("context: ra: %p, sp:%p, a0:%p", to->ra, to->sp, to->a0);

    //debug_core("save_context_and_switch_to: %p", to);
    //to->print();
    swtch(&saved_context, to);

    // debug_core("switch_back: back to saved context");
    // saved_context.print();
}

// save context and switch to another function
void cpu::save_context_and_run(std::function<void()> func) {
    kernel_assert(
        !cpu::local_irq_on() || (cpu::local_irq_on() && !(r_sie() & SIE_STIE)),
        "timer interrupt should be off");  // interrupt is off
    debugf("save_context_and_run 1: ra: %p", __builtin_return_address(0));
    context temp_context;
    memset(&temp_context, 0, sizeof(context));
    temp_context.sp = (uint64)temp_kstack;
    temp_context.ra = (uint64)__function_caller;
    temp_context.a0 = (uint64)&func;
    swtch(&saved_context, &temp_context);
    saved_context.ra = 0; // clear ra, so that we can detect if we messed up
    debugf("save_context_and_run 2: ra: %p", __builtin_return_address(0));
    
    debugf("save_context_and_run: back to saved context");
    saved_context.sp = 0; // for debug breakpoint
}


promise_base* cpu::set_promise(promise_base* p) {
    return current_process? current_process->set_promise(p): nullptr;
}


void cpu::backtrace_coroutine() {
    if (current_process) {
        current_process->backtrace_coroutine();
    }

    print_backtrace();
}

void cpu::sleep(wait_queue_base* wq, spinlock& lock) {
    kernel_assert(!cpu::local_irq_on(), "local_irq should be disabled");
    kernel_assert(current_process, "current_process should not be null");
    wq->wait_done(current_process, lock);
    
}