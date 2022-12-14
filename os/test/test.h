#ifndef TEST_TEST_H
#define TEST_TEST_H


#include <coroutine.h>
#include <utils/log.h>

#include <string>

namespace test {

class test_base {
    virtual bool run() = 0;
    virtual void print() = 0;

    protected:
    void async_failed() {
        _async_failed = true;
    }
    bool _async_failed = false;
};

}



#define __expect(var, val) \
    do { \
    if ((var) != (val)) { \
        errorf("%s:%d: expect %s == %s(%s), but got %s", __FILE__, __LINE__, #var, #val, std::to_string(val).c_str(), std::to_string(var).c_str()); \
        return false; \
    } \
    } while (0)

#define __coro_expect(var, val) \
    do { \
    if ((var) != (val)) { \
        errorf("expect %s == %s(%s), but got %s", #var, #val, std::to_string(val).c_str(), std::to_string(var).c_str()); \
        async_failed(); \
        co_return task_fail; \
    } \
    } while (0)
#endif