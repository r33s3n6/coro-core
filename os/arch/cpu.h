#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include "config.h"

#define SAMPLE_SLOT_COUNT 8

#include <ccore/types.h>
#include <arch/riscv.h>
#include <mm/layout.h>

// #include <coroutine.h>
#include <utils/panic.h>
#include <utils/utility.h>
#include <atomic/spinlock.h>


#include <functional>

class promise_base;
class wait_queue_base;


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


    // interrupt status
    // uint64 sstatus_sie = 0;

    void print();
};

// constexpr int a0_offset = (uint64)&(((context*)0)->a0);

extern "C" void swtch(context *, context *);

class cpu_ref;

// Per-CPU state.
class cpu {
    process* current_process = nullptr;         // The process running on this cpu, or null.
    context saved_context;                      // swtch() here to enter scheduler().

    int core_id;

    uint8* temp_kstack;


    uint64 sample_duration[SAMPLE_SLOT_COUNT] {};
    uint64 busy_time[SAMPLE_SLOT_COUNT] {};
    uint64 user_time[SAMPLE_SLOT_COUNT] {};  // not used

    int next_slot = 0;
    uint64 start_cycle;
    uint64 stride = 0;

    public:
    cpu(){}
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

    void set_stride(uint64 stride){
        this->stride = stride;
    }

    uint64 get_stride(){
        return stride;
    }



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


    static cpu_ref my_cpu();

    // interrupt should be disabled
    static cpu* __my_cpu(){
        return &cpus[current_id()];
    }


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

    promise_base* set_promise(promise_base* p);

    void backtrace_coroutine();

    void sleep(wait_queue_base* wq, spinlock& lock);
    void yield();
    void switch_back(context* c);
    void save_context_and_run(std::function<void()> func);
    void save_context_and_switch_to(context* next);

    uint8* get_temp_kstack() {
        return temp_kstack;
    }

    void sample(uint64 all, uint64 busy);

    
    // set halted of this core to True


    void __print();
    // task<void> print();



    // Warning: you should use these function carefully
    static bool local_irq_on(){
        return r_sstatus() & SSTATUS_SIE;
    }

    static void local_irq_enable() {
        s_sstatus(SSTATUS_SIE);
    }

    static void local_irq_disable() {
        c_sstatus(SSTATUS_SIE);
    }

    static bool local_irq_save(){
        int old = rc_sstatus(SSTATUS_SIE);
        return (old & SSTATUS_SIE);
    }

    static void local_irq_restore(bool old){
        if (old) s_sstatus(SSTATUS_SIE);
        
    }

    // actually it is not safe
    static bool __timer_irq_on() {
        return local_irq_on() && (r_sie() & SIE_STIE);
    }

    static bool timer_irq_on(){
        int old = local_irq_save();
        bool ret = (old & SSTATUS_SIE) && (r_sie() & SIE_STIE);
        local_irq_restore(old);
        return ret;
    }


    private:
    volatile bool halted = false;
    volatile bool booted = false;

};

class cpu_ref : noncopyable {
    public: //TODO: private
    cpu* c;
    int old;
    public:
    struct _Construct {
        enum Token { _Token };
    };
    // ref to current cpu
    cpu_ref(_Construct::Token) {
        old = cpu::local_irq_save();
        int id = cpu::current_id();
        c = &cpus[id];
    }
    // null ref
    cpu_ref() : c(nullptr), old(0) {}
    ~cpu_ref() {
        if(c){
            cpu::local_irq_restore(old);
        }
    }

    constexpr cpu_ref(cpu_ref&& rhs) {
        c = rhs.c;
        old = rhs.old;
        rhs.c = nullptr;
        rhs.old = 0;
    }
    constexpr cpu_ref& operator=(cpu_ref&& rhs) {
        if (c) {
            cpu::local_irq_restore(old);
        }
        c = rhs.c;
        old = rhs.old;
        rhs.c = nullptr;
        rhs.old = 0;
        return *this;
    }


    cpu* operator->() {
        return c;
    }

    const cpu* operator->() const {
        return c;
    }

    bool operator==(const cpu_ref& rhs) const {
        return c == rhs.c;
    }
};



void init_cpus();

#endif  // CPU_H
