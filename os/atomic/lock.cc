#include "lock.h"
#include <utils/log.h>


bool __debug_enabled = false;

void __debug_enable(){
    __debug_enabled = true;
}

void __debug_disable(){
    __debug_enabled = false;
}

void __debug(void* addr){
    if (__debug_enabled){
         debug_core("addr: %p", addr);
    }
   
}