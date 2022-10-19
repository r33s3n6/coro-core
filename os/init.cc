#include <mm/utils.h>
#include <mm/allocator.h>
#include <mm/vmem.h>
#include <mm/layout.h>


#include <sbi/sbi.h>
#include <utils/log.h>
#include <drivers/console.h>

#include <arch/cpu.h>

#include <cxx/icxxabi.h>

#include <trap/trap.h>


uint32 magic = 0xdeadbeef;

volatile static bool is_first = true;
volatile static int  all_started = 0;


void clean_bss()
{
	extern char s_bss[];
	extern char e_bss[];
	memset(s_bss, 0, e_bss - s_bss);
}

// deprecated
void init_globals(){
    kernel_logger.log_file = &sbi_console;
    /*
            procinit();
        binit();        // buffer cache
        inode_table_init();        // inode cache
        fileinit();     // file table
        init_trace();

        init_abstract_disk();


        init_app_names();
        init_scheduler();
        make_shell_proc();
        */

}

extern int kernel_coroutine_test();

// test code
void scheduler(){
    if (cpu::current_id() == 0){
        infof("scheduler: run kernel_coroutine_test");
        int ret = kernel_coroutine_test();
        infof("kernel_coroutine_test returned %d\n", ret);

    }else{
        infof("cpu %d is idle", cpu::current_id());
    }
}

// do not use any global class object here
// for it is not initialized yet
extern "C" void kernel_init(uint64 hartid)
{
    //if (magic != 0xdeadbeef){
		//panic("not handled exception, restarted by bootloader\n");
	//}

	//magic = 0xbeefdead;

    // set pagetables 
    if (is_first){
        is_first = false;

        // this is not dangerous only when printf<false> not access any of its member
        __infof("kernel_init");

        __infof("clean bss...");
        // init bss (stack and heap are not initialized yet)
        clean_bss();

        __infof("init globals...");
        // global constructors
        __init_cxx();

        infof("=======");
        infof("[ccore] Boot hartid=%d", hartid);
        infof("[ccore] Core count: %d", NCPU);
        infof("[ccore] s_text=%p, e_text=%p", s_text, e_text);
        infof("[ccore] s_rodata=%p, e_rodata=%p", s_rodata, e_rodata);
        infof("[ccore] s_data=%p, e_data=%p", s_data, e_data);
        infof("[ccore] s_bss_stack=%p, e_bss_stack=%p", boot_stack_top, boot_stack_bottom);
        infof("[ccore] s_bss=%p, e_bss=%p", s_bss, e_bss);

        // set cpu id
        init_cpus();
        kvminit();
        cpu::plic_init();

        init_globals();

        for (uint64 i = 0; i < NCPU; i++) {
            if (i != hartid) // not this hart
            {
                infof("[ccore] start hart %d", (uint32)i);
                start_hart(i, (uint64)_entry, 0);
            }
        }

    }

    cpu* my_cpu = cpu::my_cpu();
    my_cpu->boot_hart();
    infof("[ccore] hart %d starting", hartid);
    
    for(int i=0;i<NCPU;i++){
        while (!cpus[i].is_booted());
    }

    // all hart started
    scheduler();

    
    cpu* current_cpu = cpu::my_cpu();
    current_cpu->halt();
}

