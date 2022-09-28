#include "mm/utils.h"
#include "coroutine.h"
#include "log/printf.h"

#include "icxxabi.h"

// yield
struct data_t {
    int index;
    int value;
};
data_t test_walker_arr[9] = {{1, 1}, {2, 2}, {3, 3}, {0, 4}, {6, 5}, {4, 6}, {4, 7}, {4, 8}, {4, 9}};
task<int> test_walker(int lo, int hi) {
    for (int i = 0; i < 9; i++) {
        int index = test_walker_arr[i].index;
        if (index >= lo && index < hi) {
            co_yield test_walker_arr[i].value;
        }
    }
    co_return 0;
}

task<int> test_coroutine4_1(int* i) noexcept {
    printf("test_coroutine4.1: start\n"); 
    auto walker = test_walker(2, 5);
    printf("test_coroutine4.1: create test_walker:%p\n", walker.address()); 
    storage<int> value_stroage;
    int found = 0;
    while (true) {
        value_stroage = co_await walker;
        std::optional<int> value = value_stroage;
        if (*value == 0) {
            printf("test_coroutine4.1: walker done\n");
            break;
        }
        found+=*value;
        printf("test_coroutine4.1: walker value:%d\n",*value);

        // value = walker.promise().result;

        //printf("test_coroutine4: try to schedule out\n");
        co_await kernel_scheduler.next_schedule;

    }

    
    co_return found;
}

task<int> test_coroutine4(int* i) noexcept {
    printf("test_coroutine4: start\n"); 
    auto t = test_coroutine4_1(i);
    printf("test_coroutine4: create test_coroutine4.1:%p\n", t.address()); 

    std::optional<int> value = co_await t;

    printf("test_coroutine4: t_promise_result:%d, has_value?:%d\n",*value, value.has_value());

    assert(*value == 35, "test_coroutine4: value should be 35");

    *i+=*value;
    co_return *i;
}


task<int> test_coroutine3(int* x) noexcept {
    printf("test_coroutine3: start\n"); 
    for(int i=0;i<3;i++) {
        //printf("test_coroutine3: try to schedule out\n");
        co_await kernel_scheduler.next_schedule;
        //printf("test_coroutine3: schedule back\n");

        (*x)++;
    }

    printf("test_coroutine3: end\n"); 
    co_return task_fail;

    // co_return *x;
}

task<double> test_coroutine2(int* i) {
    printf("test_coroutine2: start\n");
    auto t= test_coroutine3(i);
    printf("test_coroutine2: test_coroutine3 address: %p\n",t.address());
    std::optional<int> x = co_await t;
    printf("test_coroutine2: after test_coroutine3: x.has_value() = %d\n", x.has_value());
    
    
    ++(*i);
    co_return *i;
}

task<void> test_coroutine1(int* i) {
    printf("test_coroutine1: start\n");
    task<double> t= test_coroutine2(i);
    printf("test_coroutine1: test_coroutine2 address: %p\n",t.address());
    std::optional<double> x = co_await t;
    printf("test_coroutine1: after test_coroutine2: x.has_value() = %d\n", x.has_value());
    
    ++(*i);
    co_return task_ok;
}

task<void> test_coroutine(int* i) {
    printf("test_coroutine: start\n");
    task<void> t= test_coroutine1(i);
    printf("test_coroutine: test_coroutine1 address: %p\n",t.address());
    bool x = co_await t;
    printf("test_coroutine: after test_coroutine1, failed?:%d\n", !x);
    //t.destroy();
    assert(*i==3, "test_coroutine: i should be 10");
    ++(*i);
    if (!x) {
        co_return task_fail;
    }

    
    co_return task_ok;
}

int main() {
    assert(false,"assert test");
    int test;
    test = 0;

    printf("main: create task\n");
    task<> h = test_coroutine(&test);
    h.promise().fast_fail=false;
    printf("main: test_coroutine address: %p\n",h.address());

    printf("main: create task4\n");
    auto h4 = test_coroutine4(&test);
    printf("main: test_coroutine4 address: %p\n",h4.address());

    kernel_scheduler.schedule(h);
    kernel_scheduler.schedule(h4);


    kernel_scheduler.start();

    assert(test==39, "test should be 3+1+35");
    return test;
}


