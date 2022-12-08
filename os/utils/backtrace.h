#ifndef UTILS_BACKTRACE_H
#define UTILS_BACKTRACE_H

#include <utils/log.h>


static inline void __print_backtrace(void* frame_address) {
    // trace stack
    debugf("backtrace stack:");
    void* frame[16] {0};
    void* ra[16] {0};
    frame[ 0] = frame_address;
    ra   [ 0] = nullptr;
    debugf("    %p: %p", frame[0], ra[0]);
    for (int i = 1; i < 16; i++) {
        if (frame[i-1] == nullptr) {
            break;
        }

        frame[i] = *(((void**)frame[i-1])-2);
        ra   [i] = *(((void**)frame[i-1])-1);
        debugf("    %p: at %p", frame[i], ra[i]);
    }
    
    
    debugf("backtrace stack done");
}

static inline void print_backtrace() {
    void* frame_address = __builtin_frame_address(0);
    __print_backtrace(frame_address);
}


#endif