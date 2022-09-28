#ifndef MM_UTILS_H
#define MM_UTILS_H

#include <bits/c++config.h>
extern "C" void* memmove(void* dest, const void* src, std::size_t n);
extern "C" void* memcpy(void* dest, const void* src, std::size_t n);
extern "C" void* memset(void* s, int c, std::size_t n);
#endif // MM_UTILS_H