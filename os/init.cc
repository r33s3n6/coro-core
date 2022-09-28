#include "mm/utils.h"
#include "rustsbi/sbi.h"
#include "log/printf.h"
#include "log/log.h"

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

extern "C" void _start();

extern "C" void kernel_init()
{
	clean_bss();
	printf("hello world!\n");
    _start();
    panic("test panic");
    /*
	proc_init();
	kinit();
	kvm_init();
	trap_init();
	plicinit();
	virtio_disk_init();
	binit();
	fsinit();
	timer_init();
	load_init_app();
	infof("start scheduler!");
	show_all_files();
	scheduler();*/
}