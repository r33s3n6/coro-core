#ifndef TEST_COROUTINE_SLEEP_TASK_HPP
#define TEST_COROUTINE_SLEEP_TASK_HPP

#include <test/test.h>

#include <utils/wait_queue.h>
#include <atomic/spinlock.h>
#include <ccore/types.h>

#include <coroutine.h>
#include <task_scheduler.h>

#include <arch/timer.h>
#include <vector>

namespace test {

namespace coroutine {






class test_sleep_task : public test_base {
private:
    uint64 ntasks;
    uint64 times;

    single_wait_queue _wait_queue;
    uint32 subtask_complete;
    uint32 subtask_total;
    spinlock lock;


    time_recorder tr;

    single_wait_queue real_test_wait_queue;
    spinlock real_test_lock;
    bool real_test_ok = false;
    bool real_test_done = false;

public:
    test_sleep_task(uint64 ntasks = 100000, uint64 times = 10000) {

        this->ntasks = ntasks;
        this->times = times;
        
    }
    bool run() override {

        kernel_task_scheduler[0].schedule(std::move(real_test()));

        real_test_lock.lock();

        while (!real_test_done) {
            cpu::my_cpu()->sleep(&real_test_wait_queue, real_test_lock);
        }

        real_test_lock.unlock();
        
        

        return real_test_ok;
    }
    void print() {
        infof("test_bdev_rw: %s, recorder:", real_test_ok ? "ok" : "fail");
        tr.print();
    }


private:
    void set_real_test_done(bool result) {
        real_test_lock.lock();
        real_test_done = true;
        real_test_ok = result;
        real_test_lock.unlock();

        real_test_wait_queue.wake_up();
        
    }

    task<void> real_test(){
        random_seed = r_time();


        tr.reset();
    
        set_subtask(ntasks);
        tr.record_timestamp("create start");
        for (uint32 i = 0; i < ntasks; i++) {
            kernel_task_scheduler[0].schedule(std::move(subtask(random())));
        }
        tr.record_timestamp("create done");
        co_await subtask_all_done();
        
        if (_async_failed) co_return task_fail;

        
        set_real_test_done(true);

        co_return task_ok;
    }

    task<void> check_async_failed(){
        if (_async_failed) {
            set_real_test_done(false);
            co_return task_fail;
        }
    }

    void set_subtask(uint32 total) {
        subtask_total = total;
        subtask_complete = 0;
    }
    void async_failed() {
        test_base::async_failed();
        subtask_done();
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

    task<void> subtask_all_done() {
        lock.lock();
        while (subtask_complete != subtask_total) {
             co_await _wait_queue.done(lock);
        }
        lock.unlock();
    }

    task<void> subtask(uint32 seed) {
        seed = complex_task(seed);

        co_await sleep_awaiter(timer::TICK_FREQ);

        seed = complex_task(seed);

        dummy += seed;

        subtask_done();

        co_return task_ok;
    }

    
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

} // namespace coroutine

} // namespace test

#endif