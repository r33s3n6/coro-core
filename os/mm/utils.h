#ifndef MM_UTILS_H
#define MM_UTILS_H

#include <bits/c++config.h>
#include <ccore/types.h>
extern "C" {
void* memmove(void* dest, const void* src, std::size_t n);
void* memcpy(void* dest, const void* src, std::size_t n);
void* memset(void* s, int c, std::size_t n);
int strlen(const char *s);
int strncmp(const char *p, const char *q, uint32 n);
char * strncpy(char *s, const char *t, int n);
char * safestrcpy(char *s, const char *t, int n);
}
#endif // MM_UTILS_H