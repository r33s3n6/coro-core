#include <arch/riscv.h>
#include <mm/layout.h>
#include <ccore/types.h>

#include <proc/process.h>

#include <utils/log.h>
#include <utils/assert.h>

#include <device/device.h>
#include <drivers/virtio/virtio_disk.h>

#include "trap.h"


void trap_init_hart() {
    enable_kernel_trap();
    w_sie((r_sie() | SIE_SEIE | SIE_SSIE) & ~SIE_STIE);
}


void set_kernel_trap() {
    w_stvec((uint64)kernel_vec & ~0x3); // DIRECT
}

void enable_kernel_trap() {
    set_kernel_trap();
    cpu::local_irq_enable();
}

void kernel_interrupt_handler(uint64 scause, uint64 stval, uint64 sepc) {
    kernel_process* p;
    uint64 cause = scause & 0xff;
    int irq;

    
    cpu* c = cpu::__my_cpu();

    switch (cause) {
    case SupervisorTimer: // kernel process timer, switch to next_context (scheduler)
        
        p = c->get_kernel_process();
        //debug_core("kernel timer interrupt: schedule %s out", p->get_name());
        c->switch_back(p->get_context());
        //debug_core("kernel timer interrupt: schedule %s in", p->get_name());
        break;
    case SupervisorExternal:
        irq = c->plic_claim();
        if (irq == VIRTIO0_IRQ) {
            // debug_core("virtio0 interrupt");
            auto dev = device::get<virtio_disk>(virtio_disk_id);
            dev->virtio_disk_intr();
            //--  virtio_disk_intr();
        } else if(irq>0) {
            warnf("unexpected interrupt irq=%d", irq);
        }
        if (irq) {
            c->plic_complete(irq);
        }
        break;
    default:
        errorf("unknown kernel interrupt: %p, sepc=%p, stval = %p\n", scause, sepc, stval);
        panic("kernel interrupt");
        break;
    }

}
uint64 last_scause; 
void __early_trace_exception(uint64 scause) {
    last_scause = scause;
}

void kernel_exception_handler(uint64 scause, uint64 stval, uint64 sepc, uint64 sp) {
    switch (scause & 0xff) {
    case InstructionMisaligned:
        errorf("InstructionMisaligned in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case InstructionAccessFault:
        errorf("SupervisorEnvCall in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case IllegalInstruction:
        errorf("IllegalInstruction in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case Breakpoint:
        errorf("Breakpoint in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case LoadMisaligned:
        errorf("LoadMisaligned in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case LoadAccessFault:
        errorf("LoadAccessFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case StoreMisaligned:
        errorf("StoreMisaligned in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case StoreAccessFault:
        errorf("StoreAccessFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case SupervisorEnvCall:
        errorf("SupervisorEnvCall in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case MachineEnvCall:
        errorf("MachineEnvCall in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case InstructionPageFault:
        __early_trace_exception(scause);
        errorf("InstructionPageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case LoadPageFault:
        __early_trace_exception(scause);
        errorf("LoadPageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case StorePageFault:
        __early_trace_exception(scause);
        errorf("StorePageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    default:
        errorf("Unknown exception in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    }
    // process* p = cpu::__my_cpu()->get_current_process();
    // if (p) {
    //     p->backtrace_coroutine();
    // }

    __trace_exception(sp);
    panic("kernel exception");
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
extern "C" void kernel_trap(uint64 sp) {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();
    uint64 stval = r_stval();



    kernel_assert(!cpu::local_irq_on(), "Interrupt can not be turned on in trap handler");
    kernel_assert((sstatus & SSTATUS_SPP) != 0, "kerneltrap: not from supervisor mode");


    if (scause & (1ULL << 63)) { 
        kernel_interrupt_handler(scause, stval, sepc);
    } else {
        kernel_exception_handler(scause, stval, sepc, sp);
    } 


    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.

    w_sepc(sepc);
    w_sstatus(sstatus);
    set_kernel_trap();
    // debugcore("About to return from kernel trap handler");

    // go back to kernelvec.S
}





void user_interrupt_handler(uint64 scause, uint64 stval, uint64 sepc) {

    cpu* c = cpu::__my_cpu();

    user_process* p = c->get_user_process();
    int irq;
    switch (scause & 0xff) {
    case SupervisorTimer:

        p->pause();
        
        break;
    case SupervisorExternal:
        irq = c->plic_claim();
        if (irq == UART0_IRQ) {
            infof("unexpected interrupt irq=UART0_IRQ");

        } else if (irq == VIRTIO0_IRQ) {
            auto dev = device::get<virtio_disk>(virtio_disk_id);
            dev->virtio_disk_intr();
        } else if (irq) {
            infof("unexpected interrupt irq=%d", irq);
        }
        if (irq) {
            c->plic_complete(irq);
        }
        break;
    default:
        infof("Unknown interrupt in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-1);
        break;
    }
}

void user_exception_handler(uint64 scause, uint64 stval, uint64 sepc) {
    cpu* c = cpu::__my_cpu();
    user_process* p = c->get_user_process();
    trapframe *trapframe = p->get_trapframe();
    switch (scause & 0xff) {
    case UserEnvCall:
        p->check_killed();
        trapframe->epc += 4;

        //-- syscall();
        break;
    case StoreAccessFault:
        infof(      "StoreAccessFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-8);
        break;
    case StorePageFault:
        infof(        "StorePageFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-7);
        break;
    case InstructionAccessFault:
        infof("InstructionAccessFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-6);
        break;
    case InstructionPageFault:
        infof(  "InstructionPageFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-5);
        break;
    case LoadAccessFault:
        infof(       "LoadAccessFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-4);
        break;
    case LoadPageFault:
        infof(         "LoadPageFault in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-2);
        break;
    case IllegalInstruction:
        errorf(   "IllegalInstruction in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-3);
        break;
    default:
        errorf(    "Unknown exception in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-1);
        break;
    }
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
void user_trap() {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();
    uint64 stval = r_stval();

    kernel_assert(!cpu::local_irq_on(), "user trap: interrupt should be off");
    kernel_assert((sstatus & SSTATUS_SPP) == 0, "usertrap: not from user mode");


    // these would be set in user_process::run

    // w_sie(r_sie() & ~SIE_STIE); // disable timer interrupt while handling user trap
    // enable_kernel_trap();
    

    if (scause & (1ULL << 63)) { // interrput = 1
        user_interrupt_handler(scause, stval, sepc);
    } else { // interrput = 0
        user_exception_handler(scause, stval, sepc);
    }

    user_trap_ret();
}

//
// return to user space
//
void user_trap_ret() {

    // we're about to switch the destination of traps from
    // kernel_trap() to user_trap(), so turn off interrupts until
    // we're back in user space, where usertrap() is correct.
    cpu::local_irq_disable();

    w_stvec(((uint64)TRAMPOLINE + (user_vec - trampoline)) & ~0x3); // DIRECT

    cpu* c = cpu::__my_cpu();

    user_process *p = (user_process*)c->get_current_process();

    trapframe *trapframe = p->get_trapframe();

    // set next trap context
    trapframe->kernel_satp =   r_satp();         // kernel page table
    trapframe->kernel_sp =     (uint64)c->get_temp_kstack();
    trapframe->kernel_trap =   (uint64)user_trap;
    trapframe->kernel_hartid = r_tp();         // save hartid

    // set up the registers that trampoline.S's sret will use
    // to get to user space.
    w_sepc(trapframe->epc);

    // set S Previous Privilege mode to User.
    uint64 x = r_sstatus();
    x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
    x |= SSTATUS_SPIE; // enable interrupts in user mode
    w_sstatus(x);

    w_sie(r_sie() | SIE_STIE | SIE_SSIE | SIE_SEIE); // enable all interrupts in user mode
    timer::set_next_timer();

    // tell trampoline.S the user page table to switch to.
    uint64 satp = MAKE_SATP(p->get_pagetable());

    // jump to trampoline.S at the top of memory, which
    // switches to the user page table, restores user registers,
    // and switches to user mode with sret.
    uint64 fn = TRAMPOLINE + (user_ret - trampoline);

    ((void (*)(uint64, uint64))fn)(TRAPFRAME, satp);
}



