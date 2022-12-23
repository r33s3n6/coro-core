#ifndef TEST_PROCESS_SLEEP_TASK_HPP
#define TEST_PROCESS_SLEEP_TASK_HPP

#include <test/test.h>



#include <utils/wait_queue.h>
#include <atomic/spinlock.h>
#include <ccore/types.h>


#include <arch/timer.h>
#include <vector>

#include <proc/process.h>
#include <proc/scheduler.h>



namespace test {

namespace process {

void __function_caller_subtask(void* arg);

class test_sleep_task : public test_base {
private:
    uint64 ntasks;
    uint64 times;

    single_wait_queue _wait_queue;
    uint32 subtask_complete;
    uint32 subtask_total;
    spinlock lock;


    time_recorder tr;


public:
    test_sleep_task(uint64 ntasks = 100000, uint64 times = 10000) {

        this->ntasks = ntasks;
        this->times = times;
        
    }
    bool run() override {

        random_seed = r_time();


        tr.reset();
    
        set_subtask(ntasks);
        tr.record_timestamp("create start");
        auto self = this;
        for (uint32 i = 0; i < ntasks; i++) {
            // test 
            
            shared_ptr<::process> test_proc;
            bool allocated = false;
            while (!allocated) {
                test_proc = make_shared<kernel_process>(kernel_process_queue.alloc_pid(), __function_caller_subtask, &self, sizeof(self));
                if ((!test_proc) || !(test_proc->waken_up())) {
                    warnf("test_sleep_task: failed to allocate process");
                    cpu::my_cpu()->yield();
                } else {
                    allocated = true;
                }
            }

            kernel_process_queue.push(test_proc);
        }
        tr.record_timestamp("create done");


        subtask_all_done();

        

        return true;
    }
    void print() {
        
        tr.print();
    }


private:




    void set_subtask(uint32 total) {
        subtask_total = total;
        subtask_complete = 0;
    }


    void subtask_done() {
        lock.lock();
        subtask_complete++;
        if (subtask_complete == subtask_total) {
            _wait_queue.wake_up();
            tr.record_timestamp("done");
        }
        lock.unlock();
    }

    void subtask_all_done() {
        lock.lock();
        while (subtask_complete != subtask_total) {
            cpu::my_cpu()->sleep(&_wait_queue, lock);
        }
        lock.unlock();
    }
public:
    void subtask(uint32 seed) {
        seed = complex_task(seed);

        cpu::my_cpu()->sleep(timer::TICK_FREQ);

        seed = complex_task(seed);

        dummy += seed;

        subtask_done();

    }
private:
    
    uint64 complex_task(uint64 seed){
        uint64 result = 0;
        for (uint64 i = 0; i < times; i++) {
            result += seed * 1103515245 + 12345;
        }
        return result;

    }

    private:
    uint64 dummy = 0;

};

void __function_caller_subtask(void* arg) {
    test_sleep_task* task = *(test_sleep_task**)arg;
    task->subtask(random());
}

} // namespace process

} // namespace test

#endif