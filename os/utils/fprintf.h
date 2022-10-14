#ifndef UTILS_FPRINTF_H
#define UTILS_FPRINTF_H

#include <coroutine.h>
#include <ccore/types.h>

#include <atomic/lock.h>
#include <fs/inode.h>

#include <mm/utils.h>



#include <stdarg.h>

#include "fprint.h"


task<void> __fprintf(file* f, const char* fmt);

template <typename format_type, typename ... Args>
task<void> __fprintf(file* f, const char* fmt, format_type x, Args ... args) {
    int c;

    int i = 0;

    // we just format one element
    while (true) {

        c = fmt[i++] & 0xff;

        if (c == '\0') {
            co_await f->write(fmt, i);
            co_return task_ok;
        }

        // wait for format specifier
        if ( c != '%' ) {
            continue;
        }

        c = fmt[i++] & 0xff;
        
        co_await f->write(fmt, i - 2);

        switch (c) {
            case 'd':
            case 'l':
            case 'x':
            case 'p':
            case 's':
                co_await __fprint(f, x, c);
                break;
            case '%':
                co_await f->write("%", 1);
                break;
            default:
                co_await f->write(fmt+i-2, 2);
                break;
        }
        break;
    }

    co_await __fprintf(f, fmt + i, args...);

    co_return task_ok;

}

template <typename ... Args>
task<void> fprintf_nolock(file* f, const char* fmt, Args ... args) {
    f->file_rw_lock.lock_off();
    co_await __fprintf(f, fmt, args...);
    f->file_rw_lock.lock_on();

    co_return task_ok;
}

template <typename ... Args>
task<void> fprintf(file* f, const char* fmt, Args ... args) {
    f->file_rw_lock.lock();
    co_await __fprintf(f, fmt, args...);
    f->file_rw_lock.unlock();
    co_return task_ok;
}


#endif // PRINTF_H
