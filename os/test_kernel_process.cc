#include <utils/log.h>
#include <utils/assert.h>
#include <proc/process.h>

int test=0;
int test2=0;

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

void test_bind_core(void* arg){

    uint64 __expect_core = *(int*)arg;

    for (uint64 i=0;i< BIG_NUMBER;i++){
        lock.lock();
        int temp = test2++;
        lock.unlock();
        if(temp % (BIG_NUMBER) == 0){

            cpu_ref c = cpu::my_cpu();
            int task_id = c->get_kernel_process()->get_pid();
            uint64 __id = c->get_core_id();
            debug_core("test_bind_core: pid:%d,expect core:%d core %d: %d", task_id, __expect_core, __id, temp);
            if (__id != __expect_core){
                debug_core("test_bind_core failed: pid: %d, expect core: %d, real_core: %d", task_id, __expect_core, __id);
                kernel_assert(false, "");
            }
        }
    }
}