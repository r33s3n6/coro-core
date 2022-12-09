
__attribute__((noreturn)) void __panic(const char* s);
__attribute__((noreturn)) void _panic(const char *s, const char* file, int line);

#define panic(s) _panic(s, __FILE__, __LINE__)