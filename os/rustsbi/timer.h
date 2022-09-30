#ifndef TIMER_H
#define TIMER_H

#include "types.h"

#define TICKS_PER_SEC (100)
// QEMU
#define CPU_FREQ (12500000)

uint64 get_cycle();
void timer_init();
void set_next_timer();
uint64 get_time_ms();
uint64 get_time_us();
uint64 get_time_ns();


#endif // TIMER_H