#include <ccore/types.h>
#include <sbi/sbi.h>
#include <atomic/spinlock.h>

#include "panic.h"

#include <stdarg.h>
static char digits[] = "0123456789abcdef";


static void
printint(int xx, int base, int sign) {
    char buf[16];
    int i;
    uint x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        sbi_console_putchar(buf[i]);
}

static void
printint64(int64 xx, int64 base, int64 sign) {
    char buf[32];
    int64 i;
    uint64 x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        sbi_console_putchar(buf[i]);
}

static void
printptr(uint64 x) {
    int i;
    sbi_console_putchar('0');
    sbi_console_putchar('x');
    for (i = 0; i < (int)(sizeof(uint64) * 2); i++, x <<= 4)
        sbi_console_putchar(digits[x >> (sizeof(uint64) * 8 - 4)]);
}

// Print to the console. only understands %d, %x, %p, %s.
int __vprintf(const char *__restrict fmt, va_list ap) {
    int i, c;
    char *s;

    for (i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            sbi_console_putchar(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0)
            break;
        switch (c) {
            case 'd':
                printint(va_arg(ap, int), 10, 1);
                break;
            case 'l':
                printint64(va_arg(ap, int64), 10, 1);
                break;
            case 'x':
                printint(va_arg(ap, int), 16, 1);
                break;
            case 'p':
                printptr(va_arg(ap, uint64));
                break;
            case 's':
                if ((s = va_arg(ap, char *)) == 0)
                    s = "(null)";
                for (; *s; s++)
                    sbi_console_putchar(*s);
                break;
            case '%':
                sbi_console_putchar('%');
                break;
            default:
                // Print unknown % sequence to draw attention.
                sbi_console_putchar('%');
                sbi_console_putchar(c);
                break;
        }
    }
    return 0;
}

// Print to the console. only understands %d, %x, %p, %s.
int __printf(const char *__restrict fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    __vprintf(fmt, ap);
    va_end(ap);

    return 0;
}

extern "C" int printf(const char *__restrict fmt, ...) {
    va_list ap;

    if (fmt == 0)
        panic("null fmt");

    va_start(ap, fmt);
    __vprintf(fmt, ap);
    va_end(ap);

    return 0;
}

