#include "mm/utils.h"
#include "rustsbi/sbi.h"
#include "log/log.h"
#include "interrupt/plic.h"

void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

void call_kernel_start();

int kernel_coroutine_test();
int kernel_start(){
	return kernel_coroutine_test();
}

extern "C" void kernel_init()
{

	clean_bss();
	printf("kernel_init: start!\n");


    /*
	proc_init();
	kinit();
	kvm_init();
	trap_init();
	*/
	plicinit();

	/*
	virtio_disk_init();
	binit();
	fsinit();
	timer_init();
	load_init_app();
	infof("start scheduler!");
	show_all_files();
	scheduler();*/

	// after init bss and enable paging, we can now do the real start
    call_kernel_start();
    shutdown();
}