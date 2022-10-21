#include <arch/riscv.h>
#include <mm/layout.h>
#include <ccore/types.h>

#include <proc/process.h>

#include <utils/log.h>
#include <utils/assert.h>

#include "trap.h"


void trap_init_hart() {
    enable_kernel_trap();
    w_sie(r_sie() | SIE_SEIE | SIE_SSIE);
}


void enable_kernel_trap() {
    w_stvec((uint64)kernel_vec & ~0x3); // DIRECT
    intr_on();
}

void kernel_interrupt_handler(uint64 scause, uint64 stval, uint64 sepc) {
    kernel_process* p;
    uint64 cause = scause & 0xff;
    int irq;
    switch (cause) {
    case SupervisorTimer: // kernel process timer, switch to next_context (scheduler)
        p = cpu::my_cpu()->get_kernel_process();
        cpu::my_cpu()->switch_back(p->get_context());
        break;
    case SupervisorExternal:
        irq = cpu::my_cpu()->plic_claim();
        if (irq == VIRTIO0_IRQ) {
            //--  virtio_disk_intr();
        } else if(irq>0) {
            warnf("unexpected interrupt irq=%d", irq);
        }
        if (irq) {
            cpu::my_cpu()->plic_complete(irq);
        }
        break;
    default:
        errorf("unknown kernel interrupt: %p, sepc=%p, stval = %p\n", scause, sepc, stval);
        panic("kernel interrupt");
        break;
    }
}


void kernel_exception_handler(uint64 scause, uint64 stval, uint64 sepc) {
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
        errorf("InstructionPageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case LoadPageFault:
        errorf("LoadPageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    case StorePageFault:
        errorf("StorePageFault in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    default:
        errorf("Unknown exception in kernel: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        break;
    }
    panic("kernel exception");
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
extern "C" void kernel_trap() {
    uint64 sepc = r_sepc();
    uint64 sstatus = r_sstatus();
    uint64 scause = r_scause();
    uint64 stval = r_stval();

    kernel_assert(!intr_get(), "Interrupt can not be turned on in trap handler");
    kernel_assert((sstatus & SSTATUS_SPP) != 0, "kerneltrap: not from supervisor mode");


    if (scause & (1ULL << 63)) { 
        kernel_interrupt_handler(scause, stval, sepc);
    } else {
        kernel_exception_handler(scause, stval, sepc);
    }

    // the yield() may have caused some traps to occur,
    // so restore trap registers for use by kernelvec.S's sepc instruction.
    w_sepc(sepc);
    w_sstatus(sstatus);
    // debugcore("About to return from kernel trap handler");

    // go back to kernelvec.S
}





void user_interrupt_handler(uint64 scause, uint64 stval, uint64 sepc) {
    user_process* p = cpu::my_cpu()->get_user_process();
    int irq;
    switch (scause & 0xff) {
    case SupervisorTimer:

        p->pause();
        
        break;
    case SupervisorExternal:
        irq = cpu::my_cpu()->plic_claim();
        if (irq == UART0_IRQ) {
            infof("unexpected interrupt irq=UART0_IRQ");

        } else if (irq == VIRTIO0_IRQ) {
            //--  virtio_disk_intr();
        } else if (irq) {
            infof("unexpected interrupt irq=%d", irq);
        }
        if (irq) {
            cpu::my_cpu()->plic_complete(irq);
        }
        break;
    default:
        infof("Unknown interrupt in user application: %p, stval = %p sepc = %p\n", scause, stval, sepc);
        p->exit(-1);
        break;
    }
}

void user_exception_handler(uint64 scause, uint64 stval, uint64 sepc) {
    user_process* p = cpu::my_cpu()->get_user_process();
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

    kernel_assert(!intr_get(), "");

    // these would be set in user_process::run

    // w_sie(r_sie() & ~SIE_STIE); // disable timer interrupt while handling user trap
    // enable_kernel_trap();

    kernel_assert((sstatus & SSTATUS_SPP) == 0, "usertrap: not from user mode");

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
    intr_off();
    w_stvec(((uint64)TRAMPOLINE + (user_vec - trampoline)) & ~0x3); // DIRECT

    cpu* c = cpu::my_cpu();

    user_process *p = (user_process*)c->get_current_process();

    trapframe *trapframe = p->get_trapframe();

    // set next trap context
    trapframe->kernel_satp =   r_satp();         // kernel page table
    trapframe->kernel_sp =     (uint64)cpu::my_cpu()->get_temp_kstack();
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



