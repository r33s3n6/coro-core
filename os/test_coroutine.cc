#include "mm/utils.h"
#include "coroutine.h"
#include "log/log.h"
#include "rustsbi/timer.h"

#include <string>




#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
void* get_stack_pointer() {
  void* p = NULL;
  return (void*)&p;
  // printf("===============stack: %p", (void*)&p);
}
#pragma GCC diagnostic pop

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
    printf("test_coroutine4.1: create test_walker:%p\n", walker.get_promise()); 

    int found = 0;
    while (true) {
        std::optional<int> value = co_await walker;

        if (*value == 0) {
            printf("test_coroutine4.1: walker done\n");
            break;
        }
        found+=*value;
        printf("test_coroutine4.1: walker value:%d\n",*value);

        // value = walker.promise().result;

        //printf("test_coroutine4: try to schedule out\n");
        // co_await kernel_scheduler.next_schedule;
        co_await this_scheduler;
    }

    
    co_return found;
}

task<int> test_coroutine4(int* i) noexcept {
    printf("test_coroutine4: start\n"); 
    auto t = test_coroutine4_1(i);
    printf("test_coroutine4: create test_coroutine4.1:%p\n", t.get_promise()); 

    std::optional<int> value = co_await t;

    printf("test_coroutine4: t_promise_result:%d, has_value?:%d\n",*value, value.has_value());

    assert(*value == 35, "test_coroutine4: value should be 35");

    *i+=*value;
    co_return *i;
}


task<int> test_coroutine3(int* x) noexcept {
    printf("test_coroutine3: start\n"); 

    //auto self = co_await get_taskbase_t{};
    
    //printf("test_coroutine3: self:%p\n", self.address());

    for(int i=0;i<3;i++) {

        //co_await kernel_scheduler.next_schedule;
        co_await this_scheduler;
        (*x)++;
    }

    printf("test_coroutine3: end\n"); 
    co_return task_fail;

    // co_return *x;
}

task<double> test_coroutine2(int* i) {
    printf("test_coroutine2: start\n");
    auto t= test_coroutine3(i);
    printf("test_coroutine2: test_coroutine3 address: %p\n",t.get_promise());
    std::optional<int> x = co_await t;
    printf("test_coroutine2: after test_coroutine3: x.has_value() = %d\n", x.has_value());

    
    ++(*i);
    co_return *i;
}

task<void> test_coroutine1(int* i) {
    printf("test_coroutine1: start\n");
    task<double> t= test_coroutine2(i);
    printf("test_coroutine1: test_coroutine2 address: %p\n",t.get_promise());
    std::optional<double> x = co_await t;
    printf("test_coroutine1: after test_coroutine2: x.has_value() = %d\n", x.has_value());
    
    ++(*i);
    co_return task_ok;
}

task<void> test_coroutine(int* i) {
    printf("test_coroutine: start\n");
    bool x;
    {
        task<void> t= test_coroutine1(i);
        t.get_promise()->has_error_handler = true;
        printf("test_coroutine: test_coroutine1 address: %p\n",t.get_promise());
        x = co_await t;
        printf("test_coroutine: after test_coroutine1, failed?:%d\n", !x);
    }
    //t.destroy();
    assert(*i==3, "test_coroutine: i should be 10");
    ++(*i);
    if (!x) {
        co_return task_fail;
    }

    
    co_return task_ok;
}

task<std::string> test_coro_string(const std::string& x){
    co_return "hello world: "+ x;
}

task<void> test_coroutine_string() {
    printf("test_coroutine_string: start\n");
    std::optional<std::string> ret = co_await test_coro_string("this is async task");
    if(ret){
        printf("test_coroutine_string: %s\n", ret->c_str());
        co_return task_ok;
    } else {
        printf("test_coroutine_string: failed\n");
        co_return task_fail;
    }


    
}

task<int> test_stack_overflow_generator(int i){
    co_return i+i;
}

task<void> test_stack_overflow(int x){
    void* ptr1 = get_stack_pointer();
    for (int i=0;i<x;i++){
        co_await test_stack_overflow_generator(i);
    }
    void* ptr2 = get_stack_pointer();
    assert(ptr1==ptr2, "stack check failed(tail call)");
    co_return task_ok;
}

task<int> test_stack_overflow2_generator(int x){
    for(int i=0;i<x ;i++){
        co_yield i;
    }
    co_return 0;
}

task<void> test_stack_overflow2(int x){
    auto gen = test_stack_overflow2_generator(x);
    void* ptr1 = get_stack_pointer();
    while(gen.get_promise()->get_status() == promise_base::status::suspend){
        co_await gen;
    }

    void* ptr2 = get_stack_pointer();
    assert(ptr1==ptr2, "stack check failed(tail call)");
    co_return task_ok;
}

task<void> test_no_return_coro(int* d){
    (*d)++;
    if (*d==0){
        co_return task_ok;
    }
}

task<void> test_no_return(){
    int d=0;
    bool ok = co_await test_no_return_coro(&d);
    printf("test_no_return: %d",ok);
    
}

struct global_test_t {
    int i;
    global_test_t() {
        i = 0xf;
    }
} global_test;

struct normal_generator{
    int x1=1;
    int x2=1;
    std::optional<int> gen(){
        int temp = x1+x2;
        x1 = x2;
        x2 = temp;
        return {temp};
    }
};

task<int> coro_generator(){
    int x1=1;
    int x2=1;
    while(true){
        int temp = x1+x2;
        x1 = x2;
        x2 = temp;
        co_yield x2;
    }
}



void test_normal_generator(int times){
    // test normal
    uint64 start_time;
    uint64 end_time;
    normal_generator ng;
    
    start_time = get_time_us();
    std::optional<int> x;
    for(int i=0;i<times;i++){
        x = ng.gen();
        if(*x==10){
            printf("don't optimize");
        }
    }
    end_time = get_time_us();
    printf("test_normal_generator: elapsed time: %dus\n", (int)(end_time-start_time));


}
task<void> test_coro_generator(int times){
    uint64 start_time;
    uint64 end_time;
    auto gen = coro_generator();
    
    start_time = get_time_us();
    std::optional<int> x;
    for(int i=0;i<times;i++){
        //gen.get_handle().resume();
        //x = gen.get_promise()->result;
        x = co_await gen;
        // printf("has?:%d",x.has_value());
        if(*x==10){
            printf("don't optimize");
        }
    }
    end_time = get_time_us();
    printf("test_coro_generator: elapsed time: %dus\n", (int)(end_time-start_time));
    co_return task_ok;
}



extern scheduler kernel_scheduler;
int kernel_coroutine_test() {
    // assert(false,"assert test");
    assert(global_test.i == 0xf, "global_test.i should be 0xf");
    int test;
    test = 0;

    printf("main: create task\n");
    auto h = test_coroutine(&test);
    // h.get_promise().fast_fail=false;
    printf("main: test_coroutine address: %p\n",h.get_promise());

    printf("main: create task4\n");
    auto h4 = test_coroutine4(&test);
    printf("main: test_coroutine4 address: %p\n",h4.get_promise());

    printf("main: create test_coroutine_string\n");
    auto h5 = test_coroutine_string();
    printf("main: test_coroutine_string address: %p\n",h5.get_promise());


    auto ho = test_stack_overflow(1000000);
    auto ho2 = test_stack_overflow2(100000);

    auto tn = test_no_return();

    auto test_coro_gen = test_coro_generator(1000000);

    kernel_scheduler.schedule(std::move(test_coro_gen));

    kernel_scheduler.schedule(std::move(ho));
    kernel_scheduler.schedule(std::move(ho2));
    kernel_scheduler.schedule(std::move(tn));

    kernel_scheduler.schedule(std::move(h));
    kernel_scheduler.schedule(std::move(h4));
    kernel_scheduler.schedule(std::move(h5));

    test_normal_generator(1000000);
    kernel_scheduler.start();
    printf("main: test: %d\n", test);
    assert(test==39, "test should be 3+1+35");
    return test;
}


