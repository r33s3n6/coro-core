#include "utils.h"

#include <ccore/types.h>

extern "C" {

void* memmove(void* dest, const void* src, std::size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    if (d < s) {
        for (std::size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (std::size_t i = n; i != 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

void* memcpy(void* dest, const void* src, std::size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (std::size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void* memset(void* s, int c, std::size_t n) {
    uint8* p = (uint8*)s;
    uint8 uc = c;
    for (std::size_t i = 0; i < n; i++) {
        p[i] = uc;
    }
    return s;
}

void* memchr(const void* s, int c, std::size_t n) {
    const uint8* p = (const uint8*)s;
    uint8 uc = c;
    for (std::size_t i = 0; i < n; i++) {
        if (p[i] == uc) {
            return (void*)(p + i);
        }
    }
    return nullptr;
}

int strlen(const char *s)
{
	int n;

	for (n = 0; s[n]; n++)
		;
	return n;
}


int memcmp(const void *v1, const void *v2, uint32 n) {
    const uint8 *s1, *s2;

    s1 = (const uint8 *)v1;
    s2 = (const uint8 *)v2;
    while (n-- > 0) {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++, s2++;
    }

    return 0;
}

char* strdup(const char* s) {
    int len = strlen(s);
    return strndup(s, len);
}

char* strndup(const char* s, uint32 n) {
    char* ns = new char[n + 1];
    memcpy(ns, s, n);
    ns[n] = 0;
    return ns;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && *s1 == *s2)
        s1++, s2++;
    return (uint8) *s1 - (uint8) *s2;
}

int strncmp(const char *p, const char *q, uint32 n) {
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (uint8) *p - (uint8) *q;
}

char* strncpy(char *s, const char *t, int n) {
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0)
        ;
    while (n-- > 0)
        *s++ = 0;
    return os;
}

// Like strncpy but guaranteed to NUL-terminate.
char* safestrcpy(char *s, const char *t, int n) {
    char *os;

    os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0;
    return os;
}

}
