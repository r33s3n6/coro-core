#ifndef SBI_SBI_H
#define SBI_SBI_H

#include <ccore/types.h>

struct sbiret {
	long error;
	long value;
};

enum sbi_ext_hsm_fid {
	SBI_EXT_HSM_HART_START = 0,
	SBI_EXT_HSM_HART_STOP,
	SBI_EXT_HSM_HART_STATUS,
};

void sbi_console_putchar(int c);
int sbi_console_getchar();
extern "C" __attribute__((noreturn)) void shutdown();
void set_timer(uint64 stime);
void start_hart(uint64 hartid, uint64 start_addr, uint64 a1);

#endif // SBI_H
