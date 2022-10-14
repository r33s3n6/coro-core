
#include "icxxabi.h"
#include <bits/c++config.h>

#include <utils/printf.h>

void __cxa_pure_virtual() {
    // Do Nothing
}

atexit_func_entry_t __atexit_funcs[ATEXIT_FUNC_MAX];
uarch_t __atexit_func_count = 0;

void *__dso_handle = 0;

int __cxa_atexit(void (*f)(void *), void *objptr, void *dso){
    if(__atexit_func_count >= ATEXIT_FUNC_MAX){
        return -1;
    }
    __atexit_funcs[__atexit_func_count].destructor_func = f;
    __atexit_funcs[__atexit_func_count].obj_ptr = objptr;
    __atexit_funcs[__atexit_func_count].dso_handle = dso;
    __atexit_func_count++;
    return 0;
}

void __cxa_finalize(void *f){
    signed i = __atexit_func_count;
    if(!f){
        while(i--){
            if(__atexit_funcs[i].destructor_func){
                (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
            }
        }
        return;
    }

    for(; i >= 0; i--){
        if(__atexit_funcs[i].destructor_func == f){
            (*__atexit_funcs[i].destructor_func)(__atexit_funcs[i].obj_ptr);
            __atexit_funcs[i].destructor_func = 0;
        }
    }
}

extern void (*__preinit_array_start []) (void) __attribute__((weak));
extern void (*__preinit_array_end []) (void) __attribute__((weak));
extern void (*__init_array_start []) (void) __attribute__((weak));
extern void (*__init_array_end []) (void) __attribute__((weak));
extern void (*__fini_array_start []) (void) __attribute__((weak));
extern void (*__fini_array_end []) (void) __attribute__((weak));


int kernel_start();


void call_kernel_start(){

    std::size_t count;
    std::size_t i;

    count = __preinit_array_end - __preinit_array_start;
    for (i = 0; i < count; i++)
        __preinit_array_start[i] ();

    count = __init_array_end - __init_array_start;
    for (i = 0; i < count; i++)
        __init_array_start[i] ();


    int ret = kernel_start();
    __printf("kernel_start returned %d\n", ret);

    count = __fini_array_end - __fini_array_start;
    for (i = 0; i < count; i++)
        __fini_array_start[i] ();

    __cxa_finalize(0);
    

}
