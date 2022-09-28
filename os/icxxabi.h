#ifndef ICXXABI_H
#define ICXXABI_H

#define ATEXIT_FUNC_MAX 128

extern "C" {


typedef unsigned uarch_t;

struct atexit_func_entry_t {
    void (*destructor_func) (void *);
    void *obj_ptr;
    void *dso_handle;

};

extern void *__dso_handle;

int __cxa_atexit(void (*f)(void *), void *objptr, void *dso);
void __cxa_finalize(void *f);

void __cxa_pure_virtual();




};


#endif//ICXXABI_H