#ifndef TEST_COROUTINE_COROUTINE_H
#define TEST_COROUTINE_COROUTINE_H

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

static uint64 random_seed = 1423;
uint64 random() {
    random_seed = random_seed * 1103515245 + 12345;
    return random_seed;
}




class time_recorder {
    struct record_t {
        uint64 timestamp;
        std::string name;
    };
private:
    std::vector<record_t> records;
    uint64 time_base;
public:
    void reset() {
        records.clear();
        time_base = timer::get_time_us();
        record_timestamp("reset");
    }
    void record_timestamp(const std::string& name) {
        records.push_back(record_t{timer::get_time_us() - time_base, name});
    }
    void print() {
        uint64 last_time = 0;
        for (auto& record : records) {
            infof("%s: %l us (diff %l us)", record.name.c_str(), record.timestamp, record.timestamp - last_time);
            last_time = record.timestamp;
        }
    }
};



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

        // TODO: destroy all buffer

        // increase random content
        set_subtask(nblocks * inc_times);
        tr.record_timestamp("inc start");
        for (uint32 i = 0; i < nblocks; i++) {
            for (uint32 j = 0; j < inc_times; j++) {
                kernel_task_scheduler[0].schedule(std::move(subtask_inc(i)));
            }
        }
        co_await subtask_all_done();
        

        if (_async_failed) co_return task_fail;

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
            auto buffer_ptr =
                *co_await kernel_block_buffer.get(test_device, block_no);
            if (!buffer_ptr) {
                panic("!buffer_ptr");
            }
            auto buffer_ref = *co_await buffer_ptr->get_ref();
            // try to get buffer for write
            uint8* buf = buffer_ref->data;
            for (int i = 0; i < 1024; i++) {
                buf[i]++;
            }
            buffer_ref->mark_dirty();
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