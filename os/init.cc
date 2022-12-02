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
#include <arch/riscv.h>

#include <cxx/icxxabi.h>

#include <trap/trap.h>
#include <utils/assert.h>

#include <coroutine.h>

#include <drivers/virtio/virtio_disk.h>
#include <drivers/ramdisk/ramdisk.h>

// uint8 __attribute__((aligned(PGSIZE))) virtio_disk_pages[2 * PGSIZE];
virtio_disk vd0((uint8*)IO_MEM_START);
ramdisk ram0(8 * 1024); // 4 MB

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
    int ret = -1;
    int retry = 3;
    while(retry && ret){
        ret = vd0.open((void*)VIRTIO0);
        retry--;
    }
    if(ret){
        panic("virtio disk open failed");
    }

    ram0.open((void*)PHYSTOP);


}

extern int kernel_coroutine_test();
extern void print_something();
extern void test_bind_core(void* arg);
extern void test_disk_rw(void* arg);
extern void test_nfs(void* arg);
extern void test_nfs2(void* arg);


// test code
void test_coroutine(){

    infof("scheduler: run kernel_coroutine_test");
    int ret = kernel_coroutine_test();
    infof("kernel_coroutine_test returned %d\n", ret);

}



void idle(){
    kernel_assert(cpu::local_irq_on() && (r_sie() & SIE_STIE), "timer interrupt should be enabled");
    warnf("%d: idle: start", cpu::my_cpu()->get_core_id());
    while (true){
        asm volatile("wfi");
    }
    kernel_assert(false, "idle should not return");
    __builtin_unreachable();
}

void init(){
    infof("init: start");

    // shared_ptr<process> test_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), (kernel_process::func_type)test_coroutine);
    // test_proc->set_name("test_coroutine");
    // kernel_process_queue.push(test_proc);

    shared_ptr<process> test_disk_rw_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), (kernel_process::func_type)test_disk_rw);
    test_disk_rw_proc->set_name("test_disk_rw");

    shared_ptr<process> test_nfs_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), test_nfs);
    test_nfs_proc->set_name("test_nfs");

    shared_ptr<process> test_nfs2_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), test_nfs2);
    test_nfs2_proc->set_name("test_nfs2");

    // kernel_process_queue.push(test_disk_rw_proc);

    kernel_process_queue.push(test_nfs2_proc);

    

    // for(int i=0;i<10;i++){
// 
    //     int bind_core;
    //     if (i<4){
    //         bind_core = i;
    //     } else {
    //         bind_core = 0;
    //     }
    //     shared_ptr<process> proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), test_bind_core, &bind_core, sizeof(bind_core));
    //     proc->set_name(("N" + std::to_string(i) + " C" + std::to_string(bind_core)).c_str());
    //     proc->binding_core = bind_core;
    //     // debug_core("init: push process %s", proc->get_name());
    //     kernel_process_queue.push(proc);
    // }



    infof("scheduler: init done");

    return;
}

void task_scheduler_run(void* arg) {
    uint64 id = *(uint64*)arg;
    kernel_assert(cpu::local_irq_on(), "task_scheduler: irq should be enabled");

    debug_core("task_scheduler: start on core %d", id);
    
    kernel_task_scheduler[id].start();
    kernel_assert(false, "task_scheduler should not return");
    __builtin_unreachable();
}

// do not use any global class object here
// for it is not initialized yet
extern "C" void kernel_init(uint64 hartid, uint64 device_tree)
{
    //if (magic != 0xdeadbeef){
		//panic("not handled exception, restarted by bootloader\n");
	//}

	//magic = 0xbeefdead;

    (void)(device_tree);

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

        // all global variables are initialized (including kernel_allocator)

        infof("=======");
        infof("[ccore] Boot hartid=%d", hartid);
        infof("[ccore] Core count: %d", NCPU);
        infof("[ccore] s_text=%p, e_text=%p", s_text, e_text);
        infof("[ccore] s_rodata=%p, e_rodata=%p", s_rodata, e_rodata);
        infof("[ccore] s_data=%p, e_data=%p", s_data, e_data);
        infof("[ccore] s_bss_stack=%p, e_bss_stack=%p", boot_stack_top, boot_stack_bottom);
        infof("[ccore] s_bss=%p, e_bss=%p", s_bss, e_bss);

        // init cpu (id and temp_stack)
        init_cpus();

        // make page table
        kvminit();

        // enable interrupts
        cpu::plic_init();


        init_globals();

        // kernel_allocator.set_debug(true);

        // start all cores
        for (uint64 i = 0; i < NCPU; i++) {
            if (i != hartid) // not this hart
            {
                infof("start hart %d", (uint32)i);
                start_hart(i, (uint64)_entry, 0);
            }
        }
    }
    
    cpu* my_cpu = cpu::__my_cpu();
    my_cpu->boot_hart();
    infof("hart %d starting", hartid);

    infof("[%d] init scheduler", hartid);
    kernel_process_scheduler[hartid].set_queue(&kernel_process_queue);


    // create idle process
    //infof("create idle process");
    shared_ptr<process> idle_proc = make_shared<kernel_process>(hartid+1, (kernel_process::func_type)idle);
    idle_proc->binding_core = hartid;
    idle_proc->set_name("idle");
    //debugf("idle_proc %p: state:%d",idle_proc.get(), idle_proc->get_state());

    //infof("set idle process");
    kernel_process_scheduler[hartid].last_choice = idle_proc;

    // create task_scheduler process
    kernel_task_scheduler[hartid].set_queue(&kernel_task_queue);
    //infof("create task_scheduler process");
    shared_ptr<process> task_scheduler_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), task_scheduler_run, (void*)&hartid, sizeof(hartid));
    task_scheduler_proc->binding_core = hartid;
    task_scheduler_proc->set_name("task_scheduler");
    //debugf("task_scheduler_proc %p: state:%d",task_scheduler_proc.get(), task_scheduler_proc->get_state());

    infof("push task_scheduler process");
    kernel_process_queue.push(task_scheduler_proc);
    

    if (hartid == 0){
        // create init process
        infof("create init process");
        shared_ptr<process> init_proc = make_shared<kernel_process>(0, (kernel_process::func_type)init);
        //infof("init_proc: ref: %d",init_proc.ref_count->count);
        init_proc->set_name("init");
        //debugf("init_proc %p: state:%d", init_proc.get(), init_proc->get_state());
        kernel_process_queue.push(init_proc);
        //infof("init_proc: ref: %d",init_proc.ref_count->count);
    }

    
    

    infof("[%d] wait for other hart", hartid);

    // wait for all hart started (TODO: seems useless)
    for(int i=0;i<NCPU;i++){
        while (!cpus[i].is_booted());
    }
    // all hart started
    



    infof("%d: start scheduler" ,hartid);
    kernel_process_scheduler[hartid].run();
    
    
    my_cpu->halt();
}

