
#include "fprintf.h"

task<void> __fprintf(file* f, const char* fmt) {
    int i = 0;

    while(true){
        char c = fmt[i++];
        if( c == '\0'){
            co_await f->write(fmt, i);
            co_return task_ok;
        }
        if (c != '%') {
            continue;
        }

        // format specifier
        c = fmt[i++];

        if(c == '%'){
            co_await f->write(fmt, i-1);
            fmt+=i;
            i=0;
        } else {
            continue;
        }

    }

    co_return task_ok;


}