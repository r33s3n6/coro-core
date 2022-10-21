#ifndef PROC_PROC_H
#define PROC_PROC_H

#include <ccore/types.h>
#include <arch/riscv.h>

#include <atomic/lock.h>

// #include <file/file.h>

#include <fs/inode.h>


#include <mm/layout.h>

#include <arch/cpu.h>
#include <trap/trap.h>


#include "exec.h"

#define NPROC (256)
#define FD_MAX (16)
#define PROC_NAME_MAX (16)
#define MAX_PROC_SHARED_MEM_INSTANCE (32)   // every proc

class process_pool;

class proc {
    friend class process_pool;
public:

    enum state {
        UNUSED = 0,
        USED,
        SLEEPING,
        RUNNABLE,
        RUNNING,
        ZOMBIE
    };
    spinlock lock {"proc.lock"};
    
    // PUBLIC: p->lock must be held when using these:
    state _state = UNUSED;  // Process state
    int pid;               // Process ID
    bool killed;            // If non-zero, have been killed
    pagetable_t pagetable; // User page table
    void *waiting_target;  // used by sleep and wakeup, a pointer of anything
    uint64 exit_code;      // Exit status to be returned to parent's wait


    proc *parent; // Parent process

    private:
    // PRIVATE: these are private to the process, so p->lock need not be held.
    uint64 ustack_bottom;        // Virtual address of user stack
    uint64 kstack_top;           // Virtual address of kernel stack (allocated by pool)
    trapframe *trapframe; // data page for trampoline.S, physical address
    context _context;      // swtch() here to run process
    // uint64 total_size;           // total memory used by this process
    uint64 bin_size;             // size of binary
    uint64 heap_size;            // heap memory used by this process
    uint64 stride;
    uint64 priority;
    uint64 cpu_time;            // ms, user and kernel
    uint64 last_start_time;     // ms
    file *files[FD_MAX]; // Opened files
    dentry *cwd;          // Current directory

/* TODO
    struct shared_mem * shmem[MAX_PROC_SHARED_MEM_INSTANCE];
    void * shmem_map_start[MAX_PROC_SHARED_MEM_INSTANCE];
    void* next_shmem_addr;
*/
    char name[PROC_NAME_MAX]; // Process name (debugging)

    int init(int pid);
    // free = clean + free_pagetable
    void free();
    // clean just do the memset
    void clean();


    static spinlock wait_lock;

    void exit(int code);
    void exec(const char *path, char *const argv[]);

    void sleep(void *waiting_target, spinlock *lk);


    public:
    file* get_file(int fd);
    int alloc_fd(file *f);

    private:
    int init_pagetable();
    void free_pagetable();
    void close_files();
};

struct process_pool {
    // static members
    proc pool[NPROC];
    spinlock pool_lock {"process_pool::pool_lock"};
    int next_pid = 1;

    process_pool();

    proc * query(int pid){
        auto guard = make_lock_guard(pool_lock);
        if(pid < 0){
            return nullptr;
        }

        for (int i = 0; i < NPROC; i++) {
            if (pool[i].pid == pid) {
                return &pool[i];
            }
        }
    }

    proc* alloc();

    void clean_children(proc *parent);
    void wake_up(void* waiting_target);

    // void free(proc* p);

};

extern process_pool proc_pool;


struct proc *findproc(int pid);


#endif // PROC_H
