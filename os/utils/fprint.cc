#include "fprint.h"
#include <mm/utils.h>

static char digits[] = "0123456789abcdef";

task<void> __fprint_int64(file* f, int64 xx, int64 base, int64 sign) {
    char buf[32];
    int64 i;
    uint64 x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 32;
    do {
        buf[--i] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[--i] = '-';

    co_await f->write(buf + i, 32 - i);

    co_return task_ok;
}

task<void> __fprint(file* f, int64 xx, char format) {
    int64 base;
    int64 sign;

    if(format == 'd') {
        base = 10;
        sign = 1;
    } else if(format == 'x') {
        base = 16;
        sign = 0;
    } else {
        co_return task_fail;
    }

    co_await __fprint_int64(f, xx, base, sign);

    co_return task_ok;

}

task<void> __fprint(file* f, void* ptr, char format) {
    char buf[32] { "0x" };
    int i;
    uint64 x = (uint64)ptr;

    if (format != 'p') {
        co_return task_fail;
    }

    for (i = 0; i < (int)(sizeof(uint64) * 2); i++, x <<= 4)
        buf[i + 2] = digits[x >> (sizeof(uint64) * 8 - 4)];

    co_await f->write(buf, i + 2);

    co_return task_ok;
        
}

task<void> __fprint(file* f, const char* str, char format) {
    const char* s;
    if (format != 's') {
        co_return task_fail;
    }

    if ((s = str) == 0)
        s = "(null)";
    co_await f->write(s, strlen(s));

    co_return task_ok;
        
}


