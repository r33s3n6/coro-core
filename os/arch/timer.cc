#include <arch/riscv.h>
#include <ccore/types.h>
#include <sbi/sbi.h>

#include "timer.h"


void timerinit() {

}

/// Set the next timer interrupt (10 ms)
void set_next_timer() {
    // 100Hz @ QEMU
    const uint64 timebase = TICK_FREQ / TIME_SLICE_PER_SEC; // how many ticks
    set_timer(r_time() + timebase);
}

void start_timer_interrupt(){
    w_sie(r_sie() | SIE_STIE);
    set_next_timer();
}

void stop_timer_interrupt(){
    w_sie(r_sie() & ~SIE_STIE);
}




uint64 get_time_ms() {
    uint64 time = r_time();
    return time / (TICK_FREQ / MSEC_PER_SEC);
}

uint64 get_time_us() {
    return r_time() * USEC_PER_SEC / TICK_FREQ;
}