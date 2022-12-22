#ifndef TEST_COROUTINE_ALLOC_HPP
#define TEST_COROUTINE_ALLOC_HPP

#include <test/test.h>
#include <task_scheduler.h>
#include <utils/wait_queue.h>

#include <mm/allocator.h>

namespace test {

namespace coroutine {

class test_alloc : public test_base {



    task<uint64> __internal_task3(int i) {
        uint64 x = 0x12345678;
        uint64 y = 0xdeadbeef;

        co_return (x^(uint64)i)-y;
    }

    task<int> __internal_task2_1(int i) {
        debugf("__internal_task2_1: before __internal_task3");
        co_return *co_await __internal_task3(i+1);
    }

    task<double> __internal_task2_2(int k) {
        int x[100];
        for (int i=0; i<100; i++) {
            x[i] = k % i;
        }

        for (int i=1; i<100; i++) {
            x[i] = x[i] + x[i-1];
        }
        debugf("__internal_task2_2: before __internal_task3");
        uint64 y = *co_await __internal_task3(k+1);

        co_return (double)x[y%100] / (double)y;


    }

    task<int> __internal_task2_3(int i) {

        debugf("__internal_task2_3: before __internal_task3");
        uint64 y = *co_await __internal_task3(i+1);

        debugf("__internal_task2_3: y=%d", y);

        co_return (int)(y%100);
        
    }

    task<int> __internal_task1(int i) {
        debugf("__internal_task1: start");
        int x,y;

        if (i<10) {
            debugf("__internal_task1: before __internal_task2_1");
            x = *co_await __internal_task2_1(i+1);
        } else {
            debugf("__internal_task1: before __internal_task2_2");
            x = (int)*co_await __internal_task2_2(i+1);
        }
        debugf("__internal_task1: before __internal_task2_3");
        y = *co_await __internal_task2_3(i+1);

        debugf("__internal_task1: x=%d, y=%d", x, y);
        co_return x+y;
    }


    public:
    test_alloc(int i) : i(i) {}

    bool run() override {

        heap_debug_output = true;
        debugf("create tasks");

        debugf("sizeof(promise<int>) = %d", sizeof(promise<int>));
        debugf("sizeof(task<int>) = %d", sizeof(task<int>));

        auto t1 = __internal_task1(i);
        //auto t2 = __internal_task2_2(i);
        //auto t3 = __internal_task3(i);

        kernel_task_scheduler[0].schedule(std::move(t1));
        //kernel_task_scheduler[0].schedule(std::move(t2));
        //kernel_task_scheduler[0].schedule(std::move(t3));

        for (int i=0; i<10000; i++) {
            cpu::my_cpu()->yield();

        }
        heap_debug_output = false;

        return true;
    }
    void print() {
        
    }

    private:


    int i = 0;


}; // class test_alloc

} // namespace coroutine

} // namespace test

#endif