#include "process.h"

#include <utils/log.h>
#include <mm/vmem.h>
#include <mm/allocator.h>


#include <utils/assert.h>
#include <arch/cpu.h>

#include <drivers/console.h>


void process::set_name(const char* name) {
    // warnf("%p:set name %s",this, name);
    strncpy(this->name, name, PROC_NAME_MAX);
}

const char* process::get_name() const{    
    // warnf("%p:get name %p",this, name);
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
        stride = cpu::my_cpu()->get_stride(); // TODO: WARNING: this is not right, but works for now
        _state = WAKEN_UP;
        
        // debugf("wake up %s", name);
    }
    lock.unlock();
}



void process::backtrace_coroutine() {
    if(current_promise) {
        current_promise->backtrace();
    } else {
        warnf("backtrace_coroutine: no current promise");
    }
}

user_process::user_process(int pid){
    // we don't need to lock p->lock here
    this->pid = pid;

    int err = __init_pagetable();

    if (err) {
        warnf("failed to create user pagetable");
        return;
    }


    this->_state = ALLOCATED; // mark as init success

}

user_process::~user_process(){
    __clean_resources();
    
}

int user_process::__mmap(uint64 va, uint64 pa, uint64 size, uint64 flags, uint64 pte_flags) {
    if (uvmmap(pagetable, va, pa, size, pte_flags) < 0) {
        warnf("cannot map va %p -> pa %p (size %l)", va, pa, size);
        return -1;
    }
    // debugf("mmap va %p -> pa %p (size %l)", va, pa, size);

    mmap_infos.push_back(mmap_info{va, size, flags, pte_flags});

    return 0;
}

int user_process::__mmap(uint64 va, uint64 size, uint64 flags, uint64 pte_flags) {
    uint64 current_va = va;
    uint64 va_end = va + size;
    while (current_va < va_end) {
        void* pa = kernel_allocator.alloc_page();
        if (pa == nullptr) {
            warnf("failed to allocate memory for user process");
            goto err_free;
        }
        if (uvmmap(pagetable, current_va, (uint64)pa, PGSIZE, pte_flags) < 0) {

            warnf("cannot map va %p -> pa %p (size %l)", va, pa, size);
            goto err_free;
        }
        current_va += PGSIZE;

    }

    mmap_infos.push_back(mmap_info{va, size, flags & ~mmap_info::MAP_SHARED, pte_flags});

    return 0;
err_free:
    if (current_va != va) {
        uvmunmap(pagetable, va, current_va - va, true);
    }
    

    return -1;

}

int user_process::__init_pagetable() {
    void* stack_pa;
    // An empty page table.

    // allocate first
    pagetable = create_empty_user_pagetable();
    trapframe_pa = (struct trapframe *)kernel_allocator.alloc_page();
    stack_pa = kernel_allocator.alloc_page();

    kernel_assert(USTACK_SIZE == PGSIZE, "USTACK_SIZE != PGSIZE");

    if (pagetable == nullptr || trapframe_pa == nullptr || stack_pa == nullptr) {
        warnf("failed to allocate memory for user process");
        free_pagetable_pages(pagetable);
        kernel_allocator.free_page(trapframe_pa);
        kernel_allocator.free_page(stack_pa);

        pagetable = nullptr;
        trapframe_pa = nullptr;
        stack_pa = nullptr;

        return -ENOMEM;
    }

    memset(trapframe_pa, 0, PGSIZE);
    memset(stack_pa, 0, PGSIZE);

    if (__mmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, mmap_info::MAP_SHARED, PTE_R | PTE_X) < 0) {
        warnf("cannot map trampoline");
        goto err_map;
    }

    if (__mmap(TRAPFRAME, (uint64)trapframe_pa, PGSIZE, mmap_info::MAP_DEFAULT, PTE_R | PTE_W) < 0) {
        warnf("cannot map trapframe");
        goto err_map;
    }

    if (__mmap(USER_STACK_TOP, (uint64)stack_pa, USTACK_SIZE, mmap_info::MAP_USER, PTE_U | PTE_R | PTE_W) < 0) {
        warnf("cannot map ustack");
        goto err_map;
    }


    stack_bottom_va = USER_STACK_BOTTOM;
    trapframe_pa->sp = stack_bottom_va;

    return 0;

err_map:
    __free_pagetable();

    return -ENOMEM;
}

// free trapframe, stack, text, heap, pagetable
void user_process::__free_pagetable() {

    for (auto& info : mmap_infos) {
        uvmunmap(pagetable, info.start, info.size, !(info.flags & mmap_info::MAP_SHARED));
    }

    mmap_infos.clear();
    
    if (pagetable)
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
    cwd = nullptr;

    // 3. clean children
    __clean_children();

    // 4. free all the memory and pagetables
    __free_pagetable();

}

// set state, wake up parent(if any)
void process::__set_exit_code(int code) {
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
void process::exit(int code){


    __clean_resources();

    lock.lock();
    __set_exit_code(code);
    lock.unlock();

    infof("proc %d exit with %d\n", pid, code);


    // kernel_assert(!cpu::local_irq_on(), "pause with irq on");
    // switch back to previous context
    cpu::__my_cpu()->switch_back(nullptr);
    __builtin_unreachable();

}

void process::__do_kill(){
    __clean_resources();
    __set_exit_code(-1);
}

void process::__kill(){

    if (_state != RUNNING) {
        __do_kill();
    } else {
        _state = KILLED; // lazy kill (by returned from run::actual_run)
    }
    
}

void process::kill(){
    lock.lock();
    __kill();
    lock.unlock();
    
}

void process::__clean_children() {
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
        kernel_assert(!cpu::local_irq_on(), "pause with irq on");
        // switch back to previous context
        cpu::__my_cpu()->switch_back(nullptr);
        __builtin_unreachable();
    }

    return false;
}

int user_process::test_load_elf(uint64 start, uint64 size){
    auto guard = make_lock_guard(lock);

    int ret = __load_elf(start, size);
    if (ret < 0) {
        warnf("__load_elf failed");
        return ret;
    }
    
    // set stdin, stdout, stderr
    files[0] = &sbi_console;
    files[1] = &sbi_console;
    files[2] = &sbi_console;

    // push argc, argv
    char name[] = "hello_world";
    uint64 sp = stack_bottom_va;
    sp -= sizeof(name);
    ret = copyout(pagetable, sp, name, sizeof(name));
    if (ret < 0) {
        warnf("copyout failed");
        return ret;
    }

    uint64 argv0 = sp; // pointer to "hello_world"
    sp -= sizeof(uint64);
    ret = copyout(pagetable, sp, &argv0, sizeof(uint64));
    if (ret < 0) {
        warnf("copyout failed");
        return ret;
    }

    uint64 argc = 1;
    sp -= sizeof(uint64);
    ret = copyout(pagetable, sp, &argc, sizeof(uint64));
    if (ret < 0) {
        warnf("copyout failed");
        return ret;
    }


    trapframe_pa->sp = sp;


    _state = WAKEN_UP;
    resume_func = user_trap_ret;
    return 0;
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
    
    kernel_assert(!cpu::timer_irq_on(), "run with irq on");

    // actual run 
    cpu::my_cpu()->save_context_and_run(resume_func);

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
        } else if (_state == SLEEPING || _state == RUNNABLE || _state == WAKEN_UP) { // process itself call sleep
            return true;
        } else {
            panic("process state is invalid");
            __builtin_unreachable();
        }
    }

    

}

// called with temp stack
void user_process::syscall_ret(uint64 ret) {
    trapframe_pa->a0 = ret;
    trapframe_pa->epc += 4;
    user_trap_ret();

}

void user_process::wait_children_done(){
    // TODO: wait children done

    // resume_func_done(0);
}

void user_process::wait_children(){
    // wait_children_queue.sleep(this);
// 
    // // old_resume_func = resume_func;
    // resume_func = std::bind(&user_process::wait_children_done, this);
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





void kernel_process::__kernel_function_caller(void(*func_ptr)(void*), void* arg) {

    //debug_core("kernel function caller: %p\n", (void*)func_ptr);


    cpu::local_irq_enable();

    func_ptr(arg);

    cpu::local_irq_disable();


    //debug_core("kernel function caller: %p done\n", (void*)func_ptr);
    
    cpu::__my_cpu()->get_kernel_process()->__set_exit_code(0);
    
    cpu::__my_cpu()->switch_back(nullptr);

    __builtin_unreachable();
}


// we would switch to __function_ptr_caller which call func, and
// if func returns, it switches back to here.
// if timer interrupt happens, it will also switch back to here.
// but leaves context valid to be continued.
kernel_process::kernel_process(int pid, func_type func, void* arg, uint64 arg_size) {

    this->pid = pid;

    void* stack_pa = kernel_allocator.alloc_page();

    if(!stack_pa) {
        return;
    }

    stack_bottom_va = (uint64)stack_pa + PGSIZE;

    uint64 stack_top_va = stack_bottom_va;
    // copy arg to stack
    if(arg) {
        stack_top_va -= arg_size;
        memcpy((void*)stack_top_va, arg, arg_size);
        // debugf("stack_top_va: %p, %p", stack_top_va, *(uint64*)stack_top_va);
    }

    memset(&_context, 0, sizeof(_context));
    _context.ra = (uint64)&kernel_process::__kernel_function_caller; 
    _context.sp = (uint64)stack_top_va;
    _context.a0 = (uint64)func;

    _state = WAKEN_UP;

    // debug_core("kernel process %d created, stack_top_va: %p\n", pid, stack_top_va);
}

void kernel_process::__clean_resources(){
    void* stack_pa = (void*)(stack_bottom_va - PGSIZE);
    kernel_allocator.free_page(stack_pa);
    stack_bottom_va = 0;

}


bool kernel_process::run(){
    {
        auto guard = make_lock_guard(lock);
        if (_state == EXITED) {
            // warnf("kernel process %d is already exited\n", pid);
            return false;
        }


        if (_state != RUNNABLE) {
            errorf("kernel process %d is not runnable: %d\n", pid, int(_state));
            
            panic("kernel process is not runnable");
        }
        

        _state = RUNNING;
    }
    
    // debug_core("_context.ra: %p", (void*)_context.ra);
    
    {
        cpu_ref c = cpu::my_cpu();
        //debug_core("run kernel process %d, state=%d", pid, (int)_state);

        // set stie, not set sie
        timer::start_timer_interrupt();
        
        c->save_context_and_switch_to(&_context);

        timer::stop_timer_interrupt();

    }


    

    {
        auto guard = make_lock_guard(lock);

        if (_state == RUNNING){ // schedule again
            _state = RUNNABLE;
            return true; 
        } else if (_state == EXITED) {
            __clean_resources();
            return false;
        } else if ((_state == SLEEPING) || (_state == RUNNABLE) || (_state == WAKEN_UP)) {
            return true; 
        } else {
            warnf("kernel process %d state is invalid: %d\n", pid, int(_state));
            panic("process state is invalid");
        }
    }
    


}