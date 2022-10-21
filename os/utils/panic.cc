#include <ccore/types.h>
#include <sbi/sbi.h>

#include "printf.h"



void panic(const char *s)
{
    __printf("panic: %s\n", s);
    shutdown();
    __builtin_unreachable();
}
