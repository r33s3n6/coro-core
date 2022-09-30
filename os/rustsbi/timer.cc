#include "timer.h"
#include "riscv.h"
#include "sbi.h"

/// read the `mtime` regiser
uint64 get_cycle()
{
	return r_time();
}

/// Enable timer interrupt
void timer_init()
{
	// Enable supervisor timer interrupt
	w_sie(r_sie() | SIE_STIE);
	set_next_timer();
}

/// Set the next timer interrupt
void set_next_timer()
{
	const uint64 timebase = CPU_FREQ / TICKS_PER_SEC;
	set_timer(get_cycle() + timebase);
}

uint64 get_time_ms()
{
	return get_cycle() / (CPU_FREQ / 1000);
}

uint64 get_time_us()
{
	return get_cycle() / (CPU_FREQ / 1000000);
}

uint64 get_time_ns()
{
	return get_cycle() / CPU_FREQ * 1000000000;
}