#ifndef TEST_PROCESS_SLEEP_HPP
#define TEST_PROCESS_SLEEP_HPP

#include <test/test.h>
#include <arch/cpu.h>

namespace test {

namespace process {

class test_sleep : public test_base {


    public:
    test_sleep(int i) : i(i) {}

    bool run() override {
        debugf("test_sleep: start at %d", r_time());
        cpu::my_cpu()->sleep(timer::TICK_FREQ); // sleep for 1 second
        debugf("test_sleep: end at %d", r_time());
        
        return true;
        
    }
    void print() {
        
    }

    private:


    int i = 0;


}; // class test_sleep

} // namespace process

} // namespace test

#endif