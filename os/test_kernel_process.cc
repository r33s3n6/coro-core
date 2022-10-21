#include <utils/log.h>

volatile int test=0;

#define BIG_NUMBER 100000000

void print_something(){
    for (int j=0;j<100;j++){
        for (int i=0;i< BIG_NUMBER;i++){
            test = test + 1;
            if(i % BIG_NUMBER/2 == 0){

                cpu* c = cpu::my_cpu();
                int id = c->get_core_id();
                debug_core("core %d: %d", id, test);
            }
        }
    }
    
}