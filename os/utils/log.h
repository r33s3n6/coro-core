#ifndef __UTILS_LOG_H__
#define __UTILS_LOG_H__

#include <arch/timer.h>
#include <fs/inode.h>
#include <arch/cpu.h>

#include <atomic/spinlock.h>
#include <coroutine.h>

#include "panic.h"

#include "fprintf.h"

#define MAX_TRACE_CNT 64


class logger {
    private:

    int64 trace_pool[NCPU][MAX_TRACE_CNT];
    int trace_last[NCPU]; // point to last write trace
    spinlock trace_lock {"trace_lock"}; // TODO: with logger name

    public:
    logger();
    ~logger() {}
    enum log_level {
        TRACE=0,
        DEBUG=1,
        INFO=2,
        WARN=3,
        ERROR=4,
    };

    enum log_color {
        // DEFAULT = 0,
        RED = 31,
        GREEN = 32,
        BROWN = 33,
        BLUE = 34,
        PURPLE = 35,
        CYAN = 36,
        GRAY = 90,
        YELLOW = 93,
    };

    constexpr static const char* log_level_str[] = {
        "TRACE",
        "DEBUG",
        "INFO",
        "WARN",
        "ERROR"
    };

    template <bool with_lock=true, typename... Args>
    task<void> printf(const char* fmt, Args... args) {
        if constexpr (with_lock) {
            log_file->file_rw_lock.lock();
        } else {
            log_file->file_rw_lock.lock_off();
        }

        co_await __fprintf(log_file, fmt, args...);

        if constexpr (with_lock) {
            log_file->file_rw_lock.unlock();
        } else {
            log_file->file_rw_lock.lock_on();
        }

        co_return task_ok;
    }

    template <bool with_lock=true, typename... Args>
    task<void> printf(enum log_level level,
                      enum log_color color,
                      const char* fmt,
                      Args... args) {
        if constexpr (with_lock) {
            log_file->file_rw_lock.lock();
        } else {
            log_file->file_rw_lock.lock_off();
        }

        co_await __fprintf(log_file, "[%d] [%s] ", get_time_us(),
                           log_level_str[level]);
        co_await __fprintf(log_file, "\033[%dm", color);
        co_await __fprintf(log_file, fmt, args...);
        co_await __fprintf(log_file, "\033[0m\n");

        if constexpr (with_lock) {
            log_file->file_rw_lock.unlock();
        } else {
            log_file->file_rw_lock.lock_on();
        }

        co_return task_ok;
    }


    void push_trace(uint64 id);

    int64 get_last_trace();

    task<void> print_trace();

    file* log_file;



};

extern logger kernel_logger;
extern logger::log_color debug_core_color[];


// Please use one of these

// #define LOG_LEVEL_NONE
#define LOG_LEVEL_CRITICAL
// #define LOG_LEVEL_DEBUG
#define LOG_LEVEL_INFO
// #define LOG_LEVEL_TRACE
// #define LOG_LEVEL_ALL

#if defined(LOG_LEVEL_CRITICAL)

#define USE_LOG_ERROR
#define USE_LOG_WARN

#endif // LOG_LEVEL_CRITICAL

#if defined(LOG_LEVEL_DEBUG)

#define USE_LOG_ERROR
#define USE_LOG_WARN
#define USE_LOG_DEBUG

#endif // LOG_LEVEL_DEBUG

#if defined(LOG_LEVEL_INFO)

#define USE_LOG_INFO

#endif // LOG_LEVEL_INFO

#if defined(LOG_LEVEL_TRACE)

#define USE_LOG_INFO
#define USE_LOG_TRACE

#endif // LOG_LEVEL_TRACE

#if defined(LOG_LEVEL_ALL)

#define USE_LOG_WARN
#define USE_LOG_ERROR
#define USE_LOG_INFO
#define USE_LOG_DEBUG
#define USE_LOG_TRACE

#endif // LOG_LEVEL_ALL





#if defined(USE_LOG_WARN)

#define warnf(fmt, ...) co_await kernel_logger.printf(logger::log_level::WARN, logger::log_color::YELLOW, fmt, ##__VA_ARGS__)
#else
#define warnf(fmt, ...)
#endif //

#if defined(USE_LOG_ERROR)

// with trace back information
#define errorf(fmt, ...)                                                                                          \
    do {                                                                                                          \
        int hartid = cpuid();                                      \
        co_await kernel_logger.printf(logger::log_level::ERROR, logger::::log_color::RED, "[%d] %s:%d: "fmt, hartid, __FILE__, __LINE__, ##__VA_ARGS__); \
        co_await kernel_logger.print_trace();                                                                       \
    } while (0)
#else
#define errorf(fmt, ...)
#endif //

#if defined(USE_LOG_DEBUG)

#define debugf(fmt, ...) co_await kernel_logger.printf(logger::log_level::DEBUG, logger::log_color::GREEN, fmt, ##__VA_ARGS__)

#define debugcore(fmt, ...)                                                                                   \
    do {                                                                                                      \
        int hartid = cpuid();                                                                                 \
        co_await kernel_logger.printf(logger::log_level::DEBUG, debug_core_color[hartid], "[%d] "fmt, hartid, ##__VA_ARGS__); \
    } while (0)

// print var in hex
#define phex(var_name) debugf(#var_name "=%p", var_name)

#else
#define debugf(fmt, ...)
#define debugcore(fmt, ...)
#define phex(var_name) (void)(var_name);
#endif //

#if defined(USE_LOG_TRACE)

#define tracef(fmt, ...) co_await kernel_logger.printf(logger::log_level::TRACE, logger::log_color::GRAY, fmt, ##__VA_ARGS__)

#define tracecore(fmt, ...)                                                                             \
    do {                                                                                                \
        uint64 hartid = cpuid();                                                                        \
        co_await kernel_logger.printf(logger::log_level::TRACE, debug_core_color[hartid], "[%d] "fmt, hartid, ##__VA_ARGS__); \
    } while (0)
#else
#define tracef(fmt, ...)
#define tracecore(fmt, ...)
#endif //

#if defined(USE_LOG_INFO)

#define infof(fmt, ...) co_await kernel_logger.printf(logger::log_level::INFO, logger::log_color::BLUE, fmt, ##__VA_ARGS__)
#else
#define infof(fmt, ...)
#endif //

#endif //!__LOG_H__
