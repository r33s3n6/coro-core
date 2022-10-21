#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#ifndef NCPU
#define NCPU 4
#endif  // NCPU

#define SAMPLE_SLOT_COUNT 8

#include <ccore/types.h>
#include <arch/riscv.h>
#include <mm/layout.h>

#include <coroutine.h>
#include <utils/panic.h>


#include <functional>


class process;
class user_process;
class kernel_process;

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

    // parameter
    uint64 a0;
};


extern "C" void swtch(context *, context *);


// Per-CPU state.
class cpu {
    process* current_process=nullptr;      // The process running on this cpu, or null.
    context saved_context;               // swtch() here to enter thread_scheduler().
    int noff=0;                         // Depth of push_off() nesting.
    int base_interrupt_status = false;  // Were interrupts enabled before push_off()?
    int core_id;

    uint8* temp_kstack;


    uint64 sample_duration[SAMPLE_SLOT_COUNT] {};
    uint64 busy_time[SAMPLE_SLOT_COUNT] {};
    uint64 user_time[SAMPLE_SLOT_COUNT] {};  // not used

    int next_slot = 0;
    uint64 start_cycle;

    public:
    cpu(){
    }
    void init(int core_id);
   public:
    //
    // the riscv Platform Level Interrupt Controller (PLIC).
    //
    static void plic_init() {
        // set desired IRQ priorities non-zero (otherwise disabled).
        *(uint32 *)(PLIC + VIRTIO0_IRQ * 4) = 1;
    }

    void boot_hart();

    bool is_booted(){
        return booted;
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


    void halt();

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

    // static proc* get_current_process() {
    //     cpu* c = my_cpu();
    //     c->push_off();
    //     proc* p = c->current_process;
    //     c->pop_off();
    //     return p;
    // }

    process* get_current_process(){
        return current_process;
    }

    user_process* get_user_process(){
        return (user_process*)current_process;
    }

    kernel_process* get_kernel_process(){
        return (kernel_process*)current_process;
    }

    void set_process(process* p){
        current_process = p;
    }

    void switch_back(context* c);
    void save_context_and_run(std::function<void()> func);
    void save_context_and_switch_to(context* next);

    uint8* get_temp_kstack() {
        return temp_kstack;
    }

    void sample(uint64 all, uint64 busy) {
                    // record one sample

        sample_duration[next_slot] = all;
        busy_time[next_slot] = busy;
        next_slot = (next_slot + 1) % SAMPLE_SLOT_COUNT;
    }

    
    // set halted of this core to True


    void __print();
    task<void> print();

    private:
    volatile bool halted = false;
    volatile bool booted = false;

};






void init_cpus();

#endif  // CPU_H
