#include "utils.h"

extern "C" void* memmove(void* dest, const void* src, std::size_t n) {
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

extern "C" void* memcpy(void* dest, const void* src, std::size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    for (std::size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

extern "C" void* memset(void* s, int c, std::size_t n) {
    char* p = (char*)s;
    for (std::size_t i = 0; i < n; i++) {
        p[i] = c;
    }
    return s;
}

extern "C" int strlen(const char *s)
{
	int n;

	for (n = 0; s[n]; n++)
		;
	return n;
}