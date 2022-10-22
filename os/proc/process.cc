#include "process.h"

#include <utils/log.h>
#include <mm/vmem.h>
#include <mm/allocator.h>


#include <utils/assert.h>


void process::set_name(const char* name) {
    warnf("%p:set name %s\n",this, name);
    strncpy(this->name, name, PROC_NAME_MAX);
}

const char* process::get_name() {    
    warnf("%p:get name %p\n",this, name);
    return name;
}

void process::sleep() {

    lock.lock();
    if(_state != RUNNING) { // killed/exited
        lock.unlock();
        return;
    }
    _state = SLEEPING;
    lock.unlock();
}

void process::wake_up() {
    lock.lock();
    if(_state == SLEEPING) {
        _state = RUNNABLE;
    }
    lock.unlock();
}

void process::pause(){
    lock.lock();
    if(_state == RUNNING) {
        _state = SLEEPING;
    }
    lock.unlock();


    cpu::my_cpu()->switch_back(nullptr); // we don't save context, for stack is shared with other processes
}

user_process::user_process(int pid){
    // we don't need to lock p->lock here
    this->pid = pid;

    int err = __init_pagetable();

    if (err) {
        warnf("failed to create user pagetable");
        return;
    }
    // memset(&_context, 0, sizeof(context));
    // memset((void *)kstack_top, 0, KSTACK_SIZE);

    // _context.ra = (uint64)forkret; // used in swtch()
    // _context.sp = kstack_top + KSTACK_SIZE;

    this->_state = ALLOCATED; // mark as init success

}

user_process::~user_process(){
    if(pagetable){
        __free_pagetable();
    }
    
}


int user_process::__init_pagetable() {
    void* stack_pa;
    // An empty page table.

    // allocate first
    pagetable = create_empty_user_pagetable();
    trapframe_pa = (struct trapframe *)kernel_allocator.alloc_page();
    stack_pa = kernel_allocator.alloc_page();

    if (pagetable == nullptr || trapframe_pa == nullptr || stack_pa == nullptr) {
        warnf("failed to allocate memory for user process");
        goto err_free;
    }

        
    if (uvmmap(pagetable, TRAMPOLINE,
                 (uint64)trampoline, PGSIZE, PTE_R | PTE_X) < 0) {

        warnf("cannot map trampoline");
        goto err_free;
    }

    // map the trapframe just below TRAMPOLINE, for trampoline.S.
    if (uvmmap(pagetable, TRAPFRAME, (uint64)(trapframe_pa),
                    PGSIZE, PTE_R | PTE_W) < 0) {
        
        warnf("Can not map trapframe");
        goto err_unmap_trampoline;
    }


    if (uvmmap(pagetable, USER_STACK_BOTTOM - USTACK_SIZE,
          (uint64)stack_pa, USTACK_SIZE, PTE_U | PTE_R | PTE_W) < 0) {
        warnf("map ustack page failed");
        goto err_unmap_trapframe;
    }

    stack_bottom_va = USER_STACK_BOTTOM;
    trapframe_pa->sp = stack_bottom_va;
    

    return 0;

err_unmap_trapframe:
    uvmunmap(pagetable, TRAPFRAME, PGSIZE, false);


err_unmap_trampoline:
    uvmunmap(pagetable, TRAMPOLINE, PGSIZE, false);

err_free:
    free_pagetable_pages(pagetable);
    kernel_allocator.free_page(trapframe_pa);
    kernel_allocator.free_page(stack_pa);

    pagetable = nullptr;
    trapframe_pa = nullptr;
    stack_pa = nullptr;

    return -ENOMEM;
}

// free trapframe, stack, text, heap, pagetable
void user_process::__free_pagetable() {
    uvmunmap(pagetable, TRAMPOLINE, PGSIZE, false);  // unmap, don't recycle physical, shared
    uvmunmap(pagetable, TRAPFRAME, PGSIZE, true);   // unmap, should recycle physical
    uvmunmap(pagetable, USER_STACK_BOTTOM - USTACK_SIZE, USTACK_SIZE, true); // unmap, should recycle physical

    trapframe_pa = nullptr;
    stack_bottom_va = 0;

    // free bin pages
    if (text_size) {
        uvmunmap(pagetable, USER_TEXT_START, PGROUNDUP(text_size), true);
        text_size = 0;
    }

/*
    if (heap_size) {
        uvmunmap(pagetable, USER_HEAP_START, PGROUNDUP(heap_size), true);
        heap_size = 0;
    }
    */

    free_pagetable_pages(pagetable);

    pagetable = nullptr;
    
}

void user_process::__close_files(){
    // TODO: free files

    // close files
    for (int i = 0; i < FD_MAX; ++i) {
        if (files[i] != nullptr) {
            // fileclose(files[i]);
            files[i] = nullptr;
        }
    }

}


void user_process::__clean_resources() {
    // 1. close files
    __close_files();

    // TODO
    // 2. put dentry
    // if (cwd) {
    //     cwd->put();
    //     cwd = nullptr;
    // }

    // 3. clean children
    __clean_children();

    // 4. free all the memory and pagetables
    __free_pagetable();

}

// set state, wake up parent(if any)
void user_process::__set_exit_code(int code) {
    exit_code = code;

    if (parent != nullptr) {
        _state = ZOMBIE;
        parent->wait_children_queue.wake_up_one();
    } else {
        _state = EXITED;
    }
}

/**
1. clean resources (files, dentry, children, memory)
2. set state, wake up parent(if any)
3. switch back to previous context (must be scheduler who runs this process)
 */
void user_process::exit(int code){


    __clean_resources();

    lock.lock();
    __set_exit_code(code);
    lock.unlock();

    infof("proc %d exit with %d\n", pid, code);

    // switch back to previous context
    cpu::my_cpu()->switch_back(nullptr);

}

void user_process::__do_kill(){
    __clean_resources();
    __set_exit_code(-1);
}

void user_process::__kill(){

    if (_state != RUNNING) {
        __do_kill();
    } else {
        _state = KILLED; // lazy kill (by returned from run::actual_run)
    }
    
}

void user_process::kill(){
    lock.lock();
    __kill();
    lock.unlock();
    
}

void user_process::__clean_children() {
    auto it = children.begin();

    while (it != children.end()) {
        auto& child = *it;

        child->lock.lock();
        child->parent = nullptr;
        child->__kill();
        child->lock.unlock();

        auto old_it = it;
        ++it;
        children.erase(old_it);
    }
}

bool user_process::check_killed(){
    auto guard = make_lock_guard(lock);

    if(_state == KILLED){
        __do_kill();
        cpu::my_cpu()->switch_back(nullptr);
        __builtin_unreachable();
    }

    return false;
}





bool user_process::run(){
    {   
        auto guard = make_lock_guard(lock);
        if (_state == EXITED || _state == ZOMBIE) {
            return false;
        }
        kernel_assert(_state == RUNNABLE, "process is not runnable");
        _state = RUNNING;
    }
    


    // actual run
    cpu::my_cpu()->save_context_and_run(resume_func);


    __sync_synchronize();

    {
        auto guard = make_lock_guard(lock);

        if(_state == KILLED) { // do lazy kill
            __do_kill();
            return false;
        } else if (_state == RUNNING){ // schedule again
            _state = RUNNABLE;
            return true; 
        } else if (_state == EXITED || _state == ZOMBIE) {
            return false;
        } else if (_state == SLEEPING) { // process itself call sleep
            return true;
        } else {
            panic("process state is invalid");
            __builtin_unreachable();
        }
    }

    

}

void user_process::resume_func_done(uint64 ret) {
    trapframe_pa->a0 = ret;
    resume_func = old_resume_func;
    resume_func();
}

void user_process::wait_children_done(){
    // TODO: wait children done

    // resume_func_done(0);
}

void user_process::wait_children(){
    wait_children_queue.sleep(this);

    old_resume_func = resume_func;
    resume_func = std::bind(&user_process::wait_children_done, this);
}

int user_process::alloc_fd(file *f) {

    for (int i = 0; i < FD_MAX; ++i) {
        if (files[i] == nullptr) {
            files[i] = f;
            return i;
        }
    }
    return -1;
}

file *user_process::get_file(int fd) {

    if (fd < 0 || fd >= FD_MAX) {
        return nullptr;
    }
    return files[fd];
}



void __kernel_function_caller(void(*func_ptr)()) {

    // debugf("kernel function caller: %p\n", (void*)func_ptr);

    // only here kernel can be interrupted by timer
    timer::start_timer_interrupt();
    intr_on();

    func_ptr();

    intr_off();
    timer::stop_timer_interrupt();

    cpu::my_cpu()->get_kernel_process()->exit();
    
    cpu::my_cpu()->switch_back(nullptr);

    __builtin_unreachable();
}


// we would switch to __function_ptr_caller which call func, and
// if func returns, it switches back to here.
// if timer interrupt happens, it will also switch back to here.
// but leaves context valid to be continued.
kernel_process::kernel_process(int pid, void (*func)()) {

    this->pid = pid;

    void* stack_pa = kernel_allocator.alloc_page();

    if(!stack_pa) {
        return;
    }

    stack_bottom_va = (uint64)stack_pa;

    memset(&_context, 0, sizeof(_context));
    _context.ra = (uint64)__kernel_function_caller; 
    _context.sp = (uint64)stack_bottom_va;
    _context.a0 = (uint64)func;

    _state = RUNNABLE;
}

void kernel_process::exit(){
    kernel_allocator.free_page((void*)stack_bottom_va);

    _state = EXITED;
    
}

bool kernel_process::run(){
    {
        auto guard = make_lock_guard(lock);
        if (_state == EXITED) {
            return false;
        }

        kernel_assert(_state == RUNNABLE, "process is not runnable");

        _state = RUNNING;
    }
    
    // actual run
    
    cpu::my_cpu()->save_context_and_switch_to(&_context);

    __sync_synchronize();

    {
        auto guard = make_lock_guard(lock);

        if (_state == RUNNING){ // schedule again
            _state = RUNNABLE;
            return true; 
        } else if (_state == EXITED) {
            return false;
        } else {
            panic("process state is invalid");
             __builtin_unreachable();
        }
    }
    


}