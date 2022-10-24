#ifndef PROC_SCHEDULER_H
#define PROC_SCHEDULER_H
#include <ccore/types.h>
#include <proc/process.h>

#include <utils/list.h>
#include <atomic/lock.h>

static const uint64 BIG_STRIDE = 0x7FFFFFFFLL;

void init_schedulers();

// TODO: use a priority queue
class task_queue {
    list<shared_ptr<process>> queue;
    spinlock lock {"task_queue.lock"};

    spinlock pid_lock {"task_queue.pid_lock"};
    int next_pid = NCPU + 1; // skip idle process and init process
public:
    shared_ptr<process> pop(int core_id);
    void push(const shared_ptr<process>& proc);
    int alloc_pid() {
        auto guard = make_lock_guard(pid_lock);
        return next_pid++;
    }
    
};

class process_scheduler {
    task_queue* shared_queue;
    public:
    // process_scheduler::process_scheduler(task_queue* q);
    void set_queue(task_queue* q) { shared_queue = q; }
    void run();
    shared_ptr<process> last_choice;
};

extern task_queue kernel_task_queue;
extern process_scheduler kernel_process_scheduler[NCPU];

#endif //PROC_SCHEDULER_H
