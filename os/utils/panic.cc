#include <ccore/types.h>
#include <sbi/sbi.h>

#include "printf.h"


void __trace_panic() {

}

void panic(const char *s)
{
    __trace_panic();
    __printf("panic: %s\n", s);
    shutdown();
    __builtin_unreachable();
}
