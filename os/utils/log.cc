#include "log.h"

using logger::log_color::BROWN;
using logger::log_color::CYAN;
using logger::log_color::GREEN;
using logger::log_color::PURPLE;

file_logger kernel_logger;
console_logger kernel_console_logger;

logger::log_color debug_core_color[] = {
    BROWN, PURPLE, CYAN,  GREEN,  BROWN, PURPLE, CYAN,  GREEN,  BROWN, PURPLE,
    CYAN,  GREEN,  BROWN, PURPLE, CYAN,  GREEN,  BROWN, PURPLE, CYAN,  GREEN,
};


file_logger::file_logger() : logger() {
    for (int i = 0; i < NCPU; i++) {
        for (int j = 0; j < MAX_TRACE_CNT; j++) {
            trace_pool[i][j] = -1;
        }
        trace_last[i] = 0;
    }
} 


void file_logger::push_trace(uint64 id) {
    lock_guard<spinlock> guard(trace_lock);

    uint64 cpu_id = cpu::current_id();
    // phex(trace_last[tp]);
    trace_last[cpu_id]++;
    trace_last[cpu_id] = trace_last[cpu_id] % MAX_TRACE_CNT;
    trace_pool[cpu_id][trace_last[cpu_id]] = id;
}

int64 file_logger::get_last_trace() {
    lock_guard<spinlock> guard(trace_lock);
    uint64 cpu_id = cpu::current_id();
    return trace_pool[cpu_id][trace_last[cpu_id]];
}

task<void> file_logger::print_trace() {
    lock_guard<spinlock> guard(trace_lock);

    uint64 cpu_id = cpu::current_id();
    int last = trace_last[cpu_id];

    log_file->file_rw_lock.lock();

    co_await __fprintf(log_file, "traceback: ");
    for (int i = 0; i < MAX_TRACE_CNT; i++) {
        co_await __fprintf(log_file, "%p ", trace_pool[cpu_id][last]);
        last--;
        if (last < 0)
            last = MAX_TRACE_CNT - 1;
    }
    co_await __fprintf(log_file, "\n");

    log_file->file_rw_lock.unlock();

    co_return task_ok;
}