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
            infof("%l us (diff %l us) (%s)", record.timestamp, record.timestamp - last_time, record.name.c_str());
            last_time = record.timestamp;
        }
    }
};

static uint64 random_seed = 1423;
uint64 random() {
    random_seed = random_seed * 1103515245 + 12345;
    return random_seed;
}


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