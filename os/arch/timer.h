#ifndef ARCH_TIMER_H
#define ARCH_TIMER_H

#include <ccore/types.h>
#include <arch/riscv.h>
#include <sbi/sbi.h>



class timer {
    
public:
    constexpr static uint64 TIME_SLICE_PER_SEC = 100;    // 10 ms 
    constexpr static uint64 MSEC_PER_SEC = 1000;    // 1s = 1000 ms
    constexpr static uint64 USEC_PER_SEC = 1000000; // 1s = 1000000 us

    constexpr static uint64 TICK_FREQ = 12500000;    // 12.5 MHz   for csr time
    constexpr static uint64 TICK_TO_MS(uint64 tick) { return (tick) / (TICK_FREQ / MSEC_PER_SEC); }
    constexpr static uint64 TICK_TO_US(uint64 tick) { return (tick) / (TICK_FREQ / USEC_PER_SEC); } // not accurate
    constexpr static uint64 MS_TO_TICK(uint64 ms) { return (ms) * (TICK_FREQ / MSEC_PER_SEC); }
    constexpr static uint64 SECOND_TO_TICK(uint64 sec) { return (sec)*TICK_FREQ; }

    constexpr static uint64 CYCLE_FREQ = 3000000000;    // 3 GHz I guess  for csr cycle
    constexpr static uint64 CYCLE_TO_MS(uint64 cycle) { return (cycle) / (CYCLE_FREQ / MSEC_PER_SEC); }
    constexpr static uint64 CYCLE_TO_US(uint64 cycle) { return (cycle) / (CYCLE_FREQ / USEC_PER_SEC); }
    constexpr static uint64 MS_TO_CYCLE(uint64 ms) { return (ms) * (CYCLE_FREQ / MSEC_PER_SEC); }
    constexpr static uint64 SECOND_TO_CYCLE(uint64 sec) { return (sec)*CYCLE_FREQ; }


    /// Set the next timer interrupt (10 ms)
    static void set_next_timer() {
        // 100Hz @ QEMU
        const uint64 timebase = TICK_FREQ / TIME_SLICE_PER_SEC; // how many ticks
        set_timer(r_time() + timebase);
    }

    static void start_timer_interrupt(){
        w_sie(r_sie() | SIE_STIE);
        set_next_timer();
    }

    static void stop_timer_interrupt(){
        w_sie(r_sie() & ~SIE_STIE);
    }

    static uint64 get_time_ms() {
        uint64 time = r_time();
        return time / (TICK_FREQ / MSEC_PER_SEC);
    }

    static uint64 get_time_us() {
        return r_time() * USEC_PER_SEC / TICK_FREQ;
    }

};


#endif // TIMER_H
