#include <utils/log.h>
#include <proc/process.h>

int test=0;

#define BIG_NUMBER (1<<22)

spinlock lock;

void print_something(){
    debug_core("print_something");
    for (int j=0;j<1;j++){
        for (uint64 i=0;i< BIG_NUMBER;i++){
            lock.lock();
            int temp = test++;
            lock.unlock();
            if(temp % (BIG_NUMBER) == 0){

                cpu_ref c = cpu::my_cpu();
                int task_id = c->get_kernel_process()->get_pid();
                int id = c->get_core_id();
                debug_core("task: %d, core %d: %d", task_id, id, temp);
            }
        }
    }
    debug_core("print_something: end");
    
}