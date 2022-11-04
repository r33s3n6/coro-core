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
protected:
    spinlock lock {"proc.lock"};
    
    
    state _state = INIT;   // Process state
    int pid = -1;                   // Process ID
    int exit_code = -1;             // Exit status to be returned to parent's wait
    uint64 stack_bottom_va = 0;     // Virtual address of stack

    process *parent = nullptr;      // Parent process
    list<shared_ptr<process>> children;
    wait_queue wait_children_queue;

    // debug
    char name[PROC_NAME_MAX] {'\0'};// Process name 

    public:
    // reschedule?
    virtual bool run() = 0;
    virtual ~process() = default;
    

    void exit(int code);
    void kill();
    

    bool ready() const{
        return _state == RUNNABLE;
    }

    bool allocated() const{
        return _state == ALLOCATED;
    }

    void pause();

    void sleep() override;
    void wake_up() override;

    void set_name(const char* name);
    const char* get_name() const;
    int get_pid() const { return pid; }
    uint64 get_stack_va() const { return stack_bottom_va; }
    state get_state() const{
        return _state;
    }

    protected:
    virtual void __clean_resources() {};


    void __set_exit_code(int code);
    void __do_kill();
    void __kill();
    void __clean_children();
};

class user_process : public process {

    protected:
    // bool killed = false;              // If non-zero, have been killed
    
    trapframe *trapframe_pa = nullptr;   // data page for trampoline.S, physical address
    pagetable_t pagetable = nullptr;  // User page table

    
    protected:
    uint64 text_size = 0;             // size of text segment (bytes)
    uint64 heap_size = 0;             // heap memory used by this process

    uint64 min_va = 0;              // min virtual address
    uint64 max_va = 0;              // max virtual address
    uint64 stack_va = 0;            // Virtual address of stack
    uint64 heap_va = 0;             // Virtual address of heap
    uint64 brk_va = 0;              // Virtual address of brk
    
    file *files[FD_MAX] {nullptr};    // Opened files
    dentry *cwd = nullptr;            // Current directory

    // void (*resume_func)(void) = nullptr; // resume function

    std::function<void()> resume_func;
    std::function<void()> old_resume_func;
    
    public:
    user_process(int pid);
    ~user_process();
    bool run() override;
    

    void exec(const char *path, char *const argv[]);
    

    void wait_children();
    bool check_killed();

    private:
    int  __init_pagetable();
    void __free_pagetable();

    void __close_files();

    int __load_text(uint64 start, uint64 end);
    int __load_elf(uint64 start, uint64 size);

    public:
    file* get_file(int fd);
    int alloc_fd(file *f);
    trapframe* get_trapframe() { return trapframe_pa; }
    pagetable_t get_pagetable() { return pagetable; }


    private:
    void wait_children_done();
    void resume_func_done(uint64 ret);

    protected:
    virtual void __clean_resources() override;
    
};


class kernel_process : public process {

    protected:
    context _context;      // swtch() here to run process

    public:
    using func_type = void (*)(void*);
    

    kernel_process(int pid, func_type func, void* arg = nullptr, uint64 arg_size = 0);
    bool run() override;
    void exit(int code);
    context* get_context() { return &_context; }

    protected:
    virtual void __clean_resources() override;

    private:
    
    static void __kernel_function_caller(void(*func_ptr)(void*), void *arg);
};



#endif // PROC_H
