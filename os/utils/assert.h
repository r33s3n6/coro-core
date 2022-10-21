#ifndef ASSERT_H
#define ASSERT_H

#include "panic.h"

// two macros ensures any macro passed will
// be expanded before being stringified
#define STRINGIZE_DETAIL(x) #x
#define STRINGIZE(x) STRINGIZE_DETAIL(x)

/**
 * @brief usage: kernel_assert(var == 1, "var should be 1");
 * 
 */
#define kernel_assert(exp, msg) \
    do {                   \
        if (!(exp))        \
            panic("Assert failed in [" __FILE__ ":" STRINGIZE(__LINE__) "]: \"" #exp "\" " msg);         \
    } while (0)

#endif // ASSERT_H
