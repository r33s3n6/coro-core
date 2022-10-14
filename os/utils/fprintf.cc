
#include "fprintf.h"

task<void> __fprintf(file* f, const char* fmt) {

    co_await f->write(fmt, strlen(fmt));
    co_return task_ok;
}