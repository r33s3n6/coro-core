#ifndef MM_UTILS_H
#define MM_UTILS_H

#include <bits/c++config.h>
#include <ccore/types.h>
extern "C" {
void* memmove(void* dest, const void* src, std::size_t n);
void* memcpy(void* dest, const void* src, std::size_t n);
void* memset(void* s, int c, std::size_t n);
char* strdup(const char* s);
char* strndup(const char* s, uint32 n);
int strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *p, const char *q, uint32 n);
char * strncpy(char *s, const char *t, int n);
char * safestrcpy(char *s, const char *t, int n);
}
#endif // MM_UTILS_H