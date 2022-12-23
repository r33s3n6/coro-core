#ifndef TEST_COROUTINE_SLEEP_HPP
#define TEST_COROUTINE_SLEEP_HPP

#include <test/test.h>
#include <arch/cpu.h>

#include <task_scheduler.h>

namespace test {

namespace coroutine {

class test_sleep : public test_base {

    task<void> __internal_task1(int id) {
        debugf("__internal_task1: %d start", id);

        for (int i=0; i<5; i++) {
            debugf("__internal_task1: %d sleep %d", id, i);
            co_await sleep_awaiter(timer::TICK_FREQ); // sleep for 1 second
        }

        debugf("__internal_task1: %d end", id);
    }

    public:
    test_sleep(int ntasks) : ntasks(ntasks) {}

    bool run() override {
        for (int i=0; i<ntasks; i++) {
            kernel_task_scheduler[0].schedule(std::move(__internal_task1(i)));
        }
        
        return true;
        
    }
    void print() {
        
    }

    private:


    int ntasks = 0;


}; // class test_sleep

} // namespace coroutine

} // namespace test

#endif