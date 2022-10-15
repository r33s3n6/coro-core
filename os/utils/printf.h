#ifndef UTILS_PRINTF_H
#define UTILS_PRINTF_H

#include <stdarg.h>
// for standard library
extern "C" int printf(const char *__restrict fmt, ...);


// for logger
int __vprintf(const char *__restrict fmt, va_list ap);
// for logger and early init process of kernel
int __printf(const char *__restrict fmt, ...);

#endif