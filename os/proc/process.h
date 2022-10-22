#ifndef PROC_PROCESS_H
#define PROC_PROCESS_H

#include <ccore/types.h>
#include <arch/riscv.h>
#include <mm/layout.h>

#include <atomic/lock.h>

#include <fs/inode.h>

#include <arch/cpu.h>
#include <trap/trap.h>

#include <utils/list.h>
#include <utils/shared_ptr.h>
#include <utils/wait_queue.h>

#include <functional>
#include <mm/utils.h>


#define FD_MAX (16)
#define PROC_NAME_MAX (16)


class process : public sleepable {

    public:
        // scheduler related
    uint64 stride = 0;
    uint64 priority = 16;
    uint64 cpu_time = 0;            // ms, user and kernel
    uint64 last_start_time = 0;     // ms
    spinlock schedule_lock {"process.schedule_lock"};

    int binding_core = -1;        // -1 means no binding

protected:

    enum state {
        INIT = 0,   
        ALLOCATED, // trapframe, stack and pagetable are allocated
        SLEEPING,
        RUNNABLE,  // text loaded
        RUNNING,
        KILLED,  // wait run to actually exit it
        ZOMBIE,
        EXITED,
    };

    spinlock lock {"proc.lock"};
    
    
    volatile state _state = INIT;   // Process state
    int pid = -1;                   // Process ID
    int exit_code = -1;             // Exit status to be returned to parent's wait
    uint64 stack_bottom_va = 0;     // Virtual address of stack



    // debug
    char name[PROC_NAME_MAX] {'\0'};// Process name 

    public:
    // reschedule?
    virtual bool run() = 0;
    virtual ~process() = default;

    bool ready() {
        return _state == RUNNABLE;
    }

    bool allocated() {
        return _state == ALLOCATED;
    }

    void pause();


    void sleep() override;
    void wake_up() override;

    void set_name(const char* name);

    const char* get_name();

};

class user_process : public process {

    protected:
    bool killed = false;              // If non-zero, have been killed
    
    trapframe *trapframe_pa = nullptr;   // data page for trampoline.S, physical address
    pagetable_t pagetable = nullptr;  // User page table

    user_process *parent = nullptr;      // Parent process
    list<shared_ptr<user_process>> children;
    wait_queue wait_children_queue;

    uint64 text_size = 0;             // size of text segment (bytes)
    uint64 heap_size = 0;             // heap memory used by this process
    
    file *files[FD_MAX] {nullptr};    // Opened files
    dentry *cwd = nullptr;            // Current directory

    // void (*resume_func)(void) = nullptr; // resume function

    std::function<void()> resume_func;
    std::function<void()> old_resume_func;
    
    public:
    user_process(int pid);
    ~user_process();
    bool run() override;
    void exit(int code);
    void kill();
    void exec(const char *path, char *const argv[]);

    void wait_children();
    bool check_killed();

    private:
    int  __init_pagetable();
    void __free_pagetable();

    void __clean_resources();
    void __close_files();
    void __clean_children();

    void __set_exit_code(int code);
    void __do_kill();
    void __kill();

    public:
    file* get_file(int fd);
    int alloc_fd(file *f);
    trapframe* get_trapframe() { return trapframe_pa; }
    pagetable_t get_pagetable() { return pagetable; }


    private:
    void wait_children_done();
    void resume_func_done(uint64 ret);
    
};


class kernel_process : public process {

    protected:
    context _context;      // swtch() here to run process

    public:
    kernel_process(int pid, void (*func)());
    bool run() override;
    void exit();
    context* get_context() { return &_context; }
};



#endif // PROC_H
