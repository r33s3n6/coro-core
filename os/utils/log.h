#ifndef __UTILS_LOG_H__
#define __UTILS_LOG_H__

#include <arch/timer.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <arch/cpu.h>

#include <atomic/spinlock.h>
#include <coroutine.h>

#include "panic.h"

#include "fprintf.h"
#include "printf.h"

#define MAX_TRACE_CNT 64


class logger {


    public:
    logger() {}
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
        DEFAULT = 39,
        BLACK = 30,
        DARK_RED = 31,
        DARK_GREEN = 32,
        DARK_YELLOW = 33,
        DARK_BLUE = 34,
        DARK_MAGENTA = 35,
        DARK_CYAN = 36,
        GRAY = 37,
        DARK_GRAY = 90,
        RED = 91,
        GREEN = 92,
        YELLOW = 93,
        BLUE = 94,
        MAGENTA = 95,
        CYAN = 96,
        WHITE = 97

    };

    constexpr static const char* log_level_str[] = {
        "TRACE",
        "DEBUG",
        " INFO",
        " WARN",
        "ERROR"
    };

    constexpr static const enum log_color log_level_color[] = {
        GRAY,
        GREEN,
        BLUE,
        YELLOW,
        RED
    };




};

class file_logger : public logger {
    private:

    int64 trace_pool[NCPU][MAX_TRACE_CNT];
    int trace_last[NCPU]; // point to last write trace
    spinlock trace_lock {"logger_trace_lock"}; // TODO: with logger name
    


    public:

    file_logger();
    template <bool with_lock=true, typename... Args>
    task<void> printf(enum log_level level,
                      enum log_color color,
                      const char* fmt,
                      Args... args) {
        if constexpr (with_lock) {
            co_await log_file->file_rw_lock.lock();
        } else {
            log_file->file_rw_lock.lock_off();
        }

        co_await __fprintf(log_file, "\033[%dm[%d] [%s] [F] ", color, timer::get_time_us(),
                           log_level_str[level]);
        co_await __fprintf(log_file, fmt, args...);
        co_await __fprintf(log_file, "\033[0m");

        if constexpr (with_lock) {
            log_file->file_rw_lock.unlock();
        } else {
            log_file->file_rw_lock.lock_on();
        }

        co_return task_ok;
    }

    template <bool with_lock=true, typename... Args>
    task<void> printf(enum log_level level,
                      const char* fmt,
                      Args... args) {

        co_await printf<with_lock>(level, log_level_color[level], fmt, args...);

        co_return task_ok;
    }

    template <bool with_lock=true, typename... Args>
    task<void> printf(const char* fmt,
                      Args... args) {

        co_await printf<with_lock>(log_level::INFO, fmt, args...);

        co_return task_ok;
    }




    void push_trace(uint64 id);

    int64 get_last_trace();

    task<void> print_trace();

    file* log_file;

};

class console_logger : public logger {
    private:
        spinlock console_printf_lock {"logger_console_printf_lock"}; // TODO: with logger name

    public:
    console_logger() : logger() {}
    ~console_logger() {}

    template <bool with_lock=true>
    int vprintf(enum log_level level,
                      enum log_color color,
                      const char* fmt, va_list args) {

        
        int ret = 0;

        bool hold = console_printf_lock.holding();
        
        if constexpr (with_lock) {
            if(!hold)
                console_printf_lock.lock();
        }

        ret |= __printf("\033[%dm[%d] [%s] [C] [%d] ", color, timer::get_time_us(),
                           log_level_str[level], cpu::current_id());

        ret |= __vprintf(fmt, args);

        ret |= __printf("\033[0m");

        if constexpr (with_lock) {
            if(!hold)
                console_printf_lock.unlock();
        }
        
        return ret;
    }


    template <bool with_lock=true>
    int printf(enum log_level level,
                      enum log_color color,
                      const char* fmt, ...) {

        va_list args;
        va_start(args, fmt);
        int ret = vprintf<with_lock>(level, color, fmt, args);
        va_end(args);
        return ret;
    }

    template <bool with_lock=true>
    int printf(enum log_level level,
                      const char* fmt, ...) {

        va_list args;
        va_start(args, fmt);
        int ret = vprintf<with_lock>(level, log_level_color[level], fmt, args);
        va_end(args);
        return ret;
    }

    template <bool with_lock=true>
    int printf(const char* fmt, ...) {

        va_list args;
        va_start(args, fmt);
        int ret = vprintf<with_lock>(log_level::INFO, log_level_color[log_level::INFO], fmt, args);
        va_end(args);
        return ret;
    }

    int raw_printf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);

        console_printf_lock.lock();
        int ret = __vprintf(fmt, args);
        console_printf_lock.unlock();

        va_end(args);
        
        return ret;
    }

    char getc() {
        while (true) {
            console_printf_lock.lock();
            int ret = sbi_console_getchar();
            console_printf_lock.unlock();
            if (ret != -1){
                return ret;
            }
        }
        
    }

    void putc(char c) {
        console_printf_lock.lock();
        sbi_console_putchar(c);
        console_printf_lock.unlock();
    }

    void lock() {
        console_printf_lock.lock();
    }

    void unlock() {
        console_printf_lock.unlock();
    }


};

extern file_logger kernel_logger;
extern console_logger kernel_console_logger;
extern logger::log_color debug_core_color[];


// Please use one of these

// #define LOG_LEVEL_NONE
#define LOG_LEVEL_CRITICAL
#define LOG_LEVEL_DEBUG
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
#define co_warnf(fmt, ...) co_await kernel_logger.printf(logger::log_level::WARN, fmt "\n", ##__VA_ARGS__)
#define __console_warnf(with_lock,fmt, ...) kernel_console_logger.printf<with_lock>(logger::log_level::WARN, fmt "\n", ##__VA_ARGS__)
#define __warnf(fmt, ...) __console_warnf(false, fmt, ##__VA_ARGS__)
#define warnf(fmt, ...) __console_warnf(true, fmt, ##__VA_ARGS__)
#else
#define co_warnf(fmt, ...)
#define __warnf(fmt, ...)
#define warnf(fmt, ...)
#endif

#if defined(USE_LOG_ERROR)

// with trace back information
#define co_errorf(fmt, ...)                                                                                          \
    do {                                                                                                          \
        int hartid = cpu::current_id();                                   \
        co_await kernel_logger.printf(logger::log_level::ERROR, "[%d] %s:%d: " fmt "\n", hartid, __FILE__, __LINE__, ##__VA_ARGS__); \
        co_await kernel_logger.print_trace();                                                                       \
    } while (0)


#define __console_errorf(with_lock, fmt, ...) kernel_console_logger.printf<with_lock>(logger::log_level::ERROR, "%s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__)
#define __errorf(fmt, ...) __console_errorf(false, fmt, ##__VA_ARGS__)    
#define errorf(fmt, ...)   __console_errorf(true, fmt, ##__VA_ARGS__)    

#else


#define co_errorf(fmt, ...)
#define __errorf(fmt, ...)
#define errorf(fmt, ...)

#endif //

#if defined(USE_LOG_DEBUG)

#define co_debugf(fmt, ...) co_await kernel_logger.printf(logger::log_level::DEBUG, fmt "\n", ##__VA_ARGS__)

#define co_debug_core(fmt, ...) co_await kernel_logger.printf(logger::log_level::DEBUG, debug_core_color[cpu::current_id()], fmt "\n", ##__VA_ARGS__)

// print var in hex
#define co_phex(var_name) co_debugf(#var_name "=%p", var_name)

#define __console_debugf(with_lock,fmt, ...) kernel_console_logger.printf<with_lock>(logger::log_level::DEBUG, fmt "\n", ##__VA_ARGS__)
#define __debugf(fmt, ...) __console_debugf(false, fmt, ##__VA_ARGS__)
// #define debugf(fmt, ...) __console_debugf(true, fmt, ##__VA_ARGS__)

#define __console_debug_core(with_lock, fmt, ...) kernel_console_logger.printf<with_lock>(logger::log_level::DEBUG, debug_core_color[cpu::current_id()], fmt "\n", ##__VA_ARGS__)

#define __debug_core(fmt, ...) __console_debug_core(false, fmt, ##__VA_ARGS__)
#define debug_core(fmt, ...) __console_debug_core(true, fmt, ##__VA_ARGS__)

#define debugf(fmt, ...) debug_core(fmt, ##__VA_ARGS__)

#define __console_phex(with_lock, var_name) __console_debugf(with_lock, #var_name "=%p", var_name)
#define __phex(var_name) __console_phex(false, var_name)
#define phex(var_name) __console_phex(true, var_name)


#else

#define co_debugf(fmt, ...)
#define co_debugcore(fmt, ...)
#define co_phex(var_name) (void)(var_name);
#define __console_debugf(fmt, ...)
#define __debugf(fmt, ...)
#define debugf(fmt, ...)
#define __console_debug_core(fmt, ...)
#define __debug_core(fmt, ...)
#define debug_core(fmt, ...)
#define __console_phex(var_name)
#define __phex(var_name)
#define phex(var_name)

#endif 

#if defined(USE_LOG_TRACE)

#define co_tracef(fmt, ...) co_await kernel_logger.printf(logger::log_level::TRACE, fmt"\n", ##__VA_ARGS__)

#define co_trace_core(fmt, ...) co_await kernel_logger.printf(logger::log_level::TRACE, debug_core_color[hartid], fmt"\n", ##__VA_ARGS__)
#else
#define co_tracef(fmt, ...)
#define co_trace_core(fmt, ...)
#endif //

#if defined(USE_LOG_INFO)

#define co_infof(fmt, ...) co_await kernel_logger.printf(logger::log_level::INFO, fmt"\n", ##__VA_ARGS__)
#define __console_infof(with_lock,fmt, ...) kernel_console_logger.printf<with_lock>(logger::log_level::INFO, fmt "\n", ##__VA_ARGS__)
#define __infof(fmt, ...) __console_infof(false, fmt, ##__VA_ARGS__)
#define infof(fmt, ...) __console_infof(true, fmt, ##__VA_ARGS__)
#else
#define co_infof(fmt, ...)
#define __console_infof(fmt, ...)
#define __infof(fmt, ...)
#define infof(fmt, ...)
#endif //

#define rawf(fmt, ...) kernel_console_logger.raw_printf(fmt "\n", ##__VA_ARGS__)
#define _rawf(fmt, ...) kernel_console_logger.raw_printf(fmt, ##__VA_ARGS__)



#endif //!__LOG_H__
