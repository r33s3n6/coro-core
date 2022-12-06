
#include <utils/panic.h>

extern "C" {
    unsigned long __stack_chk_guard = 0xBAAAAAAD;

    void __stack_chk_fail(void)                         
    {
        panic("stack smashing detected");
    }
}
