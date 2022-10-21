#include <mm/utils.h>
#include <mm/allocator.h>
#include <mm/vmem.h>
#include <mm/layout.h>

#include <proc/process.h>
#include <proc/scheduler.h>

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
extern void print_something();


// test code
void test_coroutine(){

    infof("scheduler: run kernel_coroutine_test");
    int ret = kernel_coroutine_test();
    infof("kernel_coroutine_test returned %d\n", ret);

}

void idle(){
    while (true){
        asm volatile("wfi");
    }
}

void init(){
    shared_ptr<process> test_proc = make_shared<kernel_process>(kernel_task_queue.alloc_pid(), test_coroutine);
    test_proc->set_name("test_coroutine");
    kernel_task_queue.push(test_proc);

    for(int i=0;i<10;i++){
        shared_ptr<process> proc = make_shared<kernel_process>(kernel_task_queue.alloc_pid(), print_something);
        proc->set_name(("test" + std::to_string(i)).c_str());
        kernel_task_queue.push(proc);
    }

    return;


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
                infof("start hart %d", (uint32)i);
                start_hart(i, (uint64)_entry, 0);
            }
        }

    }

    cpu* my_cpu = cpu::my_cpu();
    my_cpu->boot_hart();
    infof("hart %d starting", hartid);

    // create idle process
    
    infof("create idle process");
    shared_ptr<process> idle_proc = make_shared<kernel_process>(hartid+1, idle);
    idle_proc->binding_core = hartid;

    infof("push idle process");
    kernel_task_queue.push(idle_proc);

    if (hartid == 0){
        // create init process
        infof("create init process");
        shared_ptr<process> init_proc = make_shared<kernel_process>(0, init);
        kernel_task_queue.push(init_proc);
    }

    kernel_process_scheduler[hartid].set_queue(&kernel_task_queue);

    infof("[%d] wait for other hart", hartid);

    // wait for all hart started (TODO: seems useless)
    for(int i=0;i<NCPU;i++){
        while (!cpus[i].is_booted());
    }
    // all hart started
    


    
    infof("start scheduler");
    kernel_process_scheduler[hartid].run();

    
    cpu* current_cpu = cpu::my_cpu();
    current_cpu->halt();
}

