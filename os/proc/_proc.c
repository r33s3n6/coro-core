#include <arch/riscv.h>
#include <arch/timer.h>
#include <file/file.h>
#include <mm/layout.h>
#include <proc/proc.h>
#include <trap/trap.h>
#include <ccore/types.h>
#include <utils/log.h>
// TODO #include <mem/shared.h>

#include <mm/allocator.h>
#include <mm/vmem.h>

#include <utils/assert.h>


process_pool proc_pool;
spinlock proc::wait_lock {"proc::wait_lock"};

__attribute__((aligned(16))) char kstack[NPROC][KSTACK_SIZE];
// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.



process_pool::process_pool(){
    
    for (int i = 0; i < NPROC; i++) {
        pool[i].kstack_top = (uint64)kstack[i];
    }
    
}





/**
 * will do:
 * 1. close files and dir
 * 2. reparent this process's children
 * 3. free all the memory and pagetables
 * 4. set the state to ZOMBIE if parent is alive 
 *    or just free this proc struct if parent is dead
 */
void proc::exit(int code){
    // trace_core("exit");
    //struct proc *p = curr_proc();
    int pid_tmp = pid;   // keep for infof
    (void) pid_tmp;

    // 1. close files
    close_files();

    // 2. put dentry
    // if (cwd) {
    //     cwd->put();
    //     cwd = nullptr;
    // }
    
    lock.lock();
    exit_code = code;
    lock.unlock();


    // 2. kill all children
    //wait_lock.lock();
    proc_pool.clean_children(this);
    //wait_lock.unlock();


    // 3. free all the memory and pagetables
    free_pagetable();

    // 4. set the state
    wait_lock.lock();
    lock.lock();
    if (parent != nullptr) {
        _state = ZOMBIE;
        wakeup(parent);
    } else {
        this->clean();
    }
    lock.unlock();
    wait_lock.unlock();

    infof("proc %d exit with %d\n", pid_tmp, code);
    cpu::my_cpu()->switch_to_scheduler(&_context);
    // pushtrace(0x3032);
}


/**
 * @brief Allocate a unused proc in the pool
 * and it's initialized to some extend
 * 
 * these are not initialized or you should set them:
 * 
 * parent           nullptr
 * ustack_bottom    0
 * total_size       0
 * cwd              nullptr
 * name             ""
 * next_shmem_addr  0
 * 
 * @return struct proc* p with lock 
 */
proc* process_pool::alloc() {
    proc *p;
    int pid = -1;

    pool_lock.lock();
    for (p = pool; p < &pool[NPROC]; p++) {
        if (p->_state == proc::UNUSED) {
            p->_state = proc::USED;
            pid = next_pid++;
            break;
        }
    }
    pool_lock.unlock();

    if(pid==-1){
        return nullptr;
    }

    int err = p->init(pid);
    if(err){
        p->_state=proc::UNUSED;
        return nullptr;
    }
    
    
    return p;
}





// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void) {
    pushtrace(0x3200);
    static int first = true;
    // Still holding p->lock from scheduler.
    release(&curr_proc()->lock);

    if (first) {
        // File system initialization must be run in the context of a
        // regular process (e.g., because it calls sleep), and thus cannot
        // be run from main().
        first = false;
        fsinit();
    }

    usertrapret();
}


/**
 * @brief For debugging.
 * 
 * @param proc 
 */


/*
void print_proc(struct proc *proc) {
    printf_k("* ---------- PROC INFO ----------\n");
    printf_k("* pid:                %d\n", proc->pid);
    printf_k("* status:             ");
    if (proc->state == UNUSED) {
        printf_k("UNUSED\n");
    } else if (proc->state == USED) {
        printf_k("USED\n");
    } else if (proc->state == SLEEPING) {
        printf_k("SLEEPING\n");
    } else if (proc->state == RUNNING) {
        printf_k("RUNNING\n");
    } else if (proc->state == ZOMBIE) {
        printf_k("ZOMBIE\n");
    } else if (proc->state == RUNNABLE) {
        printf_k("RUNNABLE\n");
    } else {
        printf_k("UNKNOWN\n");
    }
    printf_k("* locked:             %d\n", proc->lock.locked);
    printf_k("* killed:             %d\n", proc->killed);
    printf_k("* pagetable:          %p\n", proc->pagetable);
    printf_k("* waiting target:     %p\n", proc->waiting_target);
    printf_k("* exit_code:          %d\n", proc->exit_code);
    printf_k("*\n");

    printf_k("* parent:             %p\n", proc->parent);
    printf_k("* ustack_bottom:      %p\n", proc->ustack_bottom);
    printf_k("* kstack:             %p\n", proc->kstack);
    printf_k("* trapframe:          %p\n", proc->trapframe);
    if(proc->trapframe){
        printf_k("*     ra:             %p\n", proc->trapframe->ra);
        printf_k("*     sp:             %p\n", proc->trapframe->sp);
        printf_k("*     epc:            %p\n", proc->trapframe->epc);
    }
    printf_k("* context:            \n");
    printf_k("*     ra:             %p\n", proc->context.ra);
    printf_k("*     sp:             %p\n", proc->context.sp);
    printf_k("* total_size:         %p\n", proc->total_size);
    printf_k("* heap_sz:            %p\n", proc->heap_sz);
    printf_k("* stride:             %p\n", proc->stride);
    printf_k("* priority:           %p\n", proc->priority);
    printf_k("* cpu_time:           %p\n", proc->cpu_time);
    printf_k("* last_time:          %p\n", proc->last_start_time);
    printf_k("* files:              \n");
    for (int i = 0; i < FD_MAX; i++) {
        if (proc->files[i] != nullptr) {
            if (i < 10) {
                printf_k("*     files[ %d]:      %p\n", i, proc->files[i]);
            } else {
                printf_k("*     files[%d]:      %p\n", i, proc->files[i]);
            }
        }
    }
    printf_k("* files:              \n");
    printf_k("* cwd:                %p\n", proc->cwd);
    printf_k("* name:               %s\n", proc->name);

    printf_k("* -------------------------------\n");
    printf_k("\n");
}

*/


void process_pool::clean_children(proc *parent) {
    proc *child;
    pool_lock.lock();

    for (child = pool; child < &pool[NPROC]; child++) {
        if (child == parent)
            continue;

        if (child->_state != proc::UNUSED && child->parent == parent) {
            // found a child
            auto guard = make_lock_guard(child->lock);
            if(child->_state == proc::ZOMBIE) {
                // child is already dead, just clean it
                child->clean();
            } else {
                // child is alive, kill it
                child->killed = true;
                child->parent = nullptr;
            }
        }
    }
    pool_lock.unlock();

}

void process_pool::wake_up(void *waiting_target) {
    struct proc *p;

    for (p = pool; p < &pool[NPROC]; p++) {
        p->lock.lock();
        if (p->_state == proc::SLEEPING && p->waiting_target == waiting_target) {
            p->_state = proc::RUNNABLE;
        }
        p->lock.unlock();
    }
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void proc::sleep(void *waiting_target, spinlock *lk) {
    
    kernel_assert(lk->holding(), "should hold lock");

    // tracecore("sleep");


    // Must acquire p->lock in order to
    // change p->state and then call switch_to_scheduler.
    // Once we hold p->lock, we can be
    // guaranteed that we won't miss any wakeup
    // (wakeup locks p->lock),
    // so it's okay to release lk.


    lock.lock();

    lk->unlock();

    // Go to sleep.
    waiting_target = waiting_target;
    _state = SLEEPING;

    lock.unlock();

    cpu::my_cpu()->switch_to_scheduler(&_context);

    // Tidy up.
    waiting_target = nullptr;

    // Reacquire original lock.
    lk->lock();

}

/**
 * Allocate a file descriptor of this process for the given file
 */
int proc::alloc_fd(file *f) {

    for (int i = 0; i < FD_MAX; ++i) {
        if (files[i] == nullptr) {
            files[i] = f;
            return i;
        }
    }
    return -1;
}

file *proc::get_file(int fd) {

    if (fd < 0 || fd >= FD_MAX) {
        return nullptr;
    }
    return files[fd];
}
