#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#ifndef NCPU
#define NCPU 1
#endif  // NCPU

#define SAMPLE_SLOT_COUNT 8

#include <ccore/types.h>
#include <arch/riscv.h>
#include <mm/layout.h>

#include <proc/thread.h>

#include <coroutine.h>
#include <utils/panic.h>


class cpu;
extern cpu cpus[NCPU];




// Saved registers for kernel context switches.
struct context {
    uint64 ra;
    uint64 sp;

    // callee-saved
    uint64 s0;
    uint64 s1;
    uint64 s2;
    uint64 s3;
    uint64 s4;
    uint64 s5;
    uint64 s6;
    uint64 s7;
    uint64 s8;
    uint64 s9;
    uint64 s10;
    uint64 s11;
};

// Per-CPU state.
class cpu {
    thread* current_thread=nullptr;     // The process running on this cpu, or null.
    struct context context;             // swtch() here to enter thread_scheduler().
    int noff=0;                         // Depth of push_off() nesting.
    int base_interrupt_status = false;  // Were interrupts enabled before push_off()?
    int core_id;


    uint64 sample_duration[SAMPLE_SLOT_COUNT] {};
    uint64 busy_time[SAMPLE_SLOT_COUNT] {};
    uint64 user_time[SAMPLE_SLOT_COUNT] {};  // not used

    int next_slot = 0;
    uint64 start_cycle;

    public:
    cpu(){
    }
    // cpu(int core_id) : core_id(core_id), start_cycle(r_time()) {}
    void init(int core_id) {
        this->core_id = core_id;
        this->start_cycle = r_time();
        __print();
    }
   public:
    //
    // the riscv Platform Level Interrupt Controller (PLIC).
    //
    static void plic_init() {
        // set desired IRQ priorities non-zero (otherwise disabled).
        *(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
    }

    void plic_init_hart() {
        // set uart's enable bit for this hart's S-mode.
        *(uint32 *)PLIC_SENABLE(core_id) = (1 << VIRTIO0_IRQ);
        // set this hart's S-mode priority threshold to 0.
        *(uint32 *)PLIC_SPRIORITY(core_id) = 0;
    }

    // ask the PLIC what interrupt we should serve.
    int plic_claim(void) {
        int irq = *(uint32 *)PLIC_SCLAIM(core_id);
        return irq;
    }

    // tell the PLIC we've served this IRQ.
    void plic_complete(int irq) {
        *(uint32 *)PLIC_SCLAIM(core_id) = irq;
    }

    void halt(){
        this->halted = true;
    }

    // push_off/pop_off are like intr_off()/intr_on() except that they are matched:
    // it takes two pop_off()s to undo two push_off()s.  Also, if interrupts
    // are initially off, then push_off, pop_off leaves them off.
    void pop_off();

    void push_off();

    int get_core_id() {
        return core_id;
    }


    // Must be called with interrupts disabled,
    // to prevent race with process being moved
    // to a different CPU.
    static int current_id() {
        int id = r_tp();
        return id;
    }

    // Return this CPU's cpu struct.
    // Interrupts must be disabled.
    static cpu* my_cpu(){
        int id = current_id();
        return &cpus[id];
    }

    
    // set halted of this core to True

    // static void halt() {
    //     cpu* c = my_cpu();
    //     c->halt();
    // }

    // Barrier
    // Will not return until all cores halted
    static void wait_all_halt() {
        for (int i = 0; i < NCPU; i++) {
            while (!cpus[i].halted)
                ;
        }
    }

    void __print();
    task<void> print();

    private:
    volatile bool halted = false;
    volatile bool booted = false;

};






void init_cpus();

#endif  // CPU_H
