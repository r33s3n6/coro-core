#include <string>

#include <utils/assert.h>
#include <utils/log.h>
#include <arch/timer.h>
#include <mm/utils.h>

#include "coroutine.h"




#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-local-addr"
void* get_stack_pointer() {
  void* p = nullptr;
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

task<int> test_coroutine4_1(int*) noexcept {
    co_infof("test_coroutine4.1: start"); 
    auto walker = test_walker(2, 5);
    co_infof("test_coroutine4.1: create test_walker:%p", walker.get_promise()); 

    int found = 0;
    while (true) {
        std::optional<int> value = co_await walker;

        if (*value == 0) {
            co_infof("test_coroutine4.1: walker done");
            break;
        }
        found+=*value;
        co_infof("test_coroutine4.1: walker value:%d",*value);

        // value = walker.promise().result;

        //printf("test_coroutine4: try to schedule out\n");
        // co_await kernel_scheduler.next_schedule;
        co_await this_scheduler;
    }


    co_return found;
}

task<void> test_coroutine4(int* i) noexcept {
    co_infof("test_coroutine4: start"); 
    auto t = test_coroutine4_1(i);
    co_infof("test_coroutine4: create test_coroutine4.1:%p", t.get_promise()); 

    std::optional<int> value = co_await t;

    co_infof("test_coroutine4: t_promise_result:%d, has_value?:%d",*value, value.has_value());

    kernel_assert(*value == 35, "test_coroutine4: value should be 35");

    *i+=*value;
    // i == 39
    co_return task_ok;
}


task<int> test_coroutine3(int* x) noexcept {
    co_infof("test_coroutine3: start"); 

    //auto self = co_await get_taskbase_t{};
    
    //printf("test_coroutine3: self:%p\n", self.address());

    for(int i=0;i<3;i++) {

        //co_await kernel_scheduler.next_schedule;
        co_await this_scheduler;
        (*x)++;
    }

    co_infof("test_coroutine3: end"); 
    co_return task_fail;

    // co_return *x;
}

task<double> test_coroutine2(int* i) {
    co_infof("test_coroutine2: start");
    auto t= test_coroutine3(i);
    co_infof("test_coroutine2: test_coroutine3 address: %p",t.get_promise());
    std::optional<int> x = co_await t;
    co_infof("test_coroutine2: after test_coroutine3: x.has_value() = %d", x.has_value());

    // unreachable
    ++(*i);
    co_return *i;
}

task<void> test_coroutine1(int* i) {
    co_infof("test_coroutine1: start");
    task<double> t= test_coroutine2(i);
    co_infof("test_coroutine1: test_coroutine2 address: %p",t.get_promise());
    std::optional<double> x = co_await t;
    co_infof("test_coroutine1: after test_coroutine2: x.has_value() = %d", x.has_value());
    
    // unreachable
    ++(*i);
    co_return task_ok;
}

task<void> test_coroutine(int* i) {

    co_infof("test_coroutine: start");
    bool x;
    {
        task<void> t= test_coroutine1(i);
        t.get_promise()->has_error_handler = true;
        co_infof("test_coroutine: test_coroutine1 address: %p",t.get_promise());
        x = co_await t;
        co_infof("test_coroutine: after test_coroutine1, failed?:%d", !x);
    }
    //t.destroy();
    kernel_assert(*i==3, "test_coroutine: i should be 3");
    ++(*i);
    if (!x) {
        co_return task_fail;
    }

    // i == 4
    co_return task_ok;
}

task<std::string> test_coro_string(const std::string& x){
    co_return "hello world: "+ x;
}

task<void> test_coroutine_string() {
    co_infof("test_coroutine_string: start");
    std::optional<std::string> ret = co_await test_coro_string("this is async task");
    if(ret){
        co_infof("test_coroutine_string: %s", ret->c_str());
        co_return task_ok;
    } else {
        co_infof("test_coroutine_string: failed");
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
    kernel_assert(ptr1==ptr2, "stack check failed(tail call)");
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
    kernel_assert(ptr1==ptr2, "stack check failed(tail call)");
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
    co_infof("test_no_return: %d",ok);
    
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
    kernel_console_logger.printf("coro_generator: start\n");
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
    
    start_time = timer::get_time_us();
    std::optional<int> x;
    for(int i=0;i<times;i++){
        x = ng.gen();
        if(*x==10){
            kernel_console_logger.printf("don't optimize");
        }
    }
    end_time = timer::get_time_us();
    kernel_console_logger.printf("test_normal_generator: elapsed time: %dus\n", (int)(end_time-start_time));


}
task<void> test_coro_generator(int times){
    kernel_console_logger.printf("test_coro_generator: start\n");
    uint64 start_time;
    uint64 end_time;
    auto gen = coro_generator();
    
    start_time = timer::get_time_us();
    std::optional<int> x;
    for(int i=0;i<times;i++){
        //gen.get_handle().resume();
        //x = gen.get_promise()->result;
        x = co_await gen;
        // printf("has?:%d",x.has_value());
        if(*x==10){
            co_infof("don't optimize");
        }
    }
    end_time = timer::get_time_us();
    co_infof("test_coro_generator: elapsed time: %dus", (int)(end_time-start_time));
    co_return task_ok;
}

task<void> test_logger(std::string log_str){
    kernel_console_logger.printf("test_logger: start\n");
    co_await kernel_logger.printf("%%\n");
    uint64 a = -1;
    co_await kernel_logger.printf("test logger: %d %p %x %s %% %i\n",a,&a,a,"test");
    co_await kernel_logger.printf("test logger: %s\n",log_str.c_str());
    kernel_console_logger.printf("test_logger: end\n");
    co_return task_ok;
}

struct test_coroutine_kill_struct {
    int * ref;
    test_coroutine_kill_struct(int* ref):ref(ref){}
    ~test_coroutine_kill_struct(){
        infof("test_coroutine_kill_struct: destructor");
        (*ref)++;
    }
};

task<int> test_coroutine_kill_callee(int* ref){
    (*ref)++;
    co_await this_scheduler;
    co_return task_fail;
}

task<void> test_coroutine_kill(int* ref){
    
    test_coroutine_kill_struct t(ref);
    co_await test_coroutine_kill_callee(ref);
    co_errorf("test_coroutine_kill: this should not be printed");
    co_await test_coroutine_kill_callee(ref);
    co_return task_ok;
}

// extern task_scheduler kernel_task_scheduler[NCPU];
task_scheduler test_scheduler;
task_queue test_queue;

int kernel_coroutine_test() {
    // assert(false,"assert test");
    kernel_assert(global_test.i == 0xf, "global_test.i should be 0xf");
    int test;
    test = 0;

    kernel_console_logger.printf("main: create task\n");
    auto h = test_coroutine(&test);
    // h.get_promise().fast_fail=false;
    kernel_console_logger.printf("main: test_coroutine address: %p\n",h.get_promise());

    kernel_console_logger.printf("main: create task4\n");
    auto h4 = test_coroutine4(&test);
    kernel_console_logger.printf("main: test_coroutine4 address: %p\n",h4.get_promise());

    kernel_console_logger.printf("main: create test_coroutine_string\n");
    auto h5 = test_coroutine_string();
    kernel_console_logger.printf("main: test_coroutine_string address: %p\n",h5.get_promise());

    auto t_logger = test_logger("hello world");


    auto ho = test_stack_overflow(100000);
    auto ho2 = test_stack_overflow2(100000);

    auto tn = test_no_return();

    auto test_coro_gen = test_coro_generator(1000000);

    int test_coroutine_kill_ref = 0;
    auto test_coroutine_kill_task = test_coroutine_kill(&test_coroutine_kill_ref);

    test_scheduler.set_queue(&test_queue);


    test_scheduler.schedule(std::move(t_logger));

    test_scheduler.schedule(std::move(test_coro_gen));

    test_scheduler.schedule(std::move(ho));
    test_scheduler.schedule(std::move(ho2));
    test_scheduler.schedule(std::move(tn));

    test_scheduler.schedule(std::move(h));
    test_scheduler.schedule(std::move(h4));
    test_scheduler.schedule(std::move(h5));

    test_scheduler.schedule(std::move(test_coroutine_kill_task));


    test_normal_generator(1000000);

    kernel_console_logger.printf("main: test scheduler start\n");
    test_scheduler.start();

    kernel_console_logger.printf("main: test: %d\n", test);
    kernel_assert(test==39, "test should be 3+1+35");
    kernel_assert(test_coroutine_kill_ref==2, "test_coroutine_kill_ref should be 2");
    return test;
}


