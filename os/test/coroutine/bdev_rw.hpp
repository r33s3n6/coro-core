#ifndef TEST_COROUTINE_BDEV_RW_HPP
#define TEST_COROUTINE_BDEV_RW_HPP

#include <test/test.h>

#include <device/device.h>
#include <device/block/bdev.h>
#include <device/block/buf.h>

#include <utils/wait_queue.h>
#include <atomic/spinlock.h>
#include <ccore/types.h>

#include <coroutine.h>
#include <task_scheduler.h>

#include <arch/timer.h>
#include <vector>

namespace test {

namespace coroutine {




class test_bdev_rw : public test_base {
private:
    block_device* test_device;

    single_wait_queue _wait_queue;
    uint32 subtask_complete;
    uint32 subtask_total;
    spinlock lock;

    uint32 nblocks;
    uint32 inc_times;

    char* random_data; // 1024 bytes

    time_recorder tr;

    single_wait_queue real_test_wait_queue;
    spinlock real_test_lock;
    bool real_test_ok = false;
    bool real_test_done = false;

public:
    test_bdev_rw(device_id_t test_device_id, uint32 _nblocks = 1024, uint32 _inc_times = 32) {
        test_device = device::get<block_device>(test_device_id);
        nblocks = _nblocks;
        inc_times = _inc_times;
        random_data = new char[1024];
        for (int i = 0; i < 1024; i++) {
            random_data[i] = random();
        }
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
        tr.reset();


        
        // all set random content
        set_subtask(nblocks);
        tr.record_timestamp("first write start");
        for (uint32 i = 0; i < nblocks; i++) {
            kernel_task_scheduler[0].schedule(std::move(subtask_write(i)));
        }
        co_await subtask_all_done();
        
        if (_async_failed) co_return task_fail;

        uint32 failed_count = *co_await kernel_block_buffer.destroy(test_device->device_id);
        tr.record_timestamp("destroy done");

        debugf("failed_count: %d, size: %d", failed_count, kernel_block_buffer.size());

        // increase random content
        set_subtask(nblocks * inc_times);
        tr.record_timestamp("inc start");
        for (uint32 i = 0; i < nblocks; i++) {
            for (uint32 j = 0; j < inc_times; j++) {
                kernel_task_scheduler[0].schedule(std::move(subtask_inc(i)));
            }
            if (i % 16 == 15) {
                debugf("i: %d", i);
            }
        }
        co_await subtask_all_done();

        if (_async_failed) co_return task_fail;

        //co_await kernel_block_buffer.destroy(test_device->device_id);
        //tr.record_timestamp("destroy done");

        // read and check
        set_subtask(nblocks);
        tr.record_timestamp("check start");
        for (uint32 i = 0; i < nblocks; i++) {
            kernel_task_scheduler[0].schedule(std::move(subtask_check(i)));
        }
        co_await subtask_all_done();


        
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

    task<void> subtask_write(uint32 block_no) {
        {
            auto buffer_ptr =
                *co_await kernel_block_buffer.get(test_device, block_no);
            auto buffer_ref = *co_await buffer_ptr->get_ref();
            
            // try to get buffer for write
            uint8* buf = buffer_ref->data;
            memmove(buf, random_data, 1024);
            buffer_ref->mark_dirty();
        }


        subtask_done();

        co_return task_ok;
    }

    task<void> subtask_inc(uint32 block_no) {
        {
            auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, block_no);
            // if (!buffer_ptr) {
            //     warnf("!buffer_ptr, buffer_ptr_opt.has_value(): %d", buffer_ptr_opt.has_value());
            //     panic("!buffer_ptr");
            // }
            // if (!buffer_ptr->data) {
            //     panic("!buffer_ptr->data");
            // }
            // auto buffer_ref = *co_await buffer_ptr->get_ref();

            co_await buffer_ptr->get();

            // try to get buffer for write
            for (int i = 0; i < 1024; i++) {
                buffer_ptr->data[i]++;
            }
            buffer_ptr->mark_dirty();
            buffer_ptr->put();
        }


        subtask_done();

        co_return task_ok;
    }

    task<void> subtask_check(uint32 block_no) {
        {
            auto buffer_ptr =
                *co_await kernel_block_buffer.get(test_device, block_no);
            auto buffer_ref = *co_await buffer_ptr->get_ref();
            // try to get buffer for read
            const uint8* buf = buffer_ref->data;

            for (int i = 0; i < 1024; i++) {
               __coro_expect(buf[i], (uint8)(random_data[i] + inc_times));
            }
        }

        subtask_done();

        co_return task_ok;
    }

    



};

} // namespace coroutine

} // namespace test

#endif