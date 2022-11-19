# coro-core 开发日志

## 概述

**题目** 异步协程内核

**目标描述**

希望在内核的部分或全部组件内引入协程，长耗时任务使用异步调用，以使逻辑更加清晰并提升性能，提供异步系统调用接口。目前打算以uCore-SMP作为基础，使用C++编写。

**仓库** [fuzx20/coro-core](https://git.tsinghua.edu.cn/fuzx20/coro-core)

**日志链接** [开发日志](https://git.tsinghua.edu.cn/fuzx20/coro-core/-/blob/master/dev_log.md)

### 参考实现

- 2021春季学期：陶天骅的操作系统课程设计 uCore-SMP
  - [源代码仓库和文档](https://github.com/TianhuaTao/uCore-SMP)
  - [pdf文档](http://taotianhua.com/ucore-smp/doc)
- 2022年操作系统比赛：南开大学对uCore-SMP的推进，在U740上支持多核和SD卡
  - [uCore-SMP-NKU](https://github.com/NKU-EmbeddedSystem/uCore-SMP)



## 开发日志

### 第3周 (9.26-10.2)

#### 进展

1. 配置环境，使得C++可以编译出不依赖标准库、异常、RTTI的逻辑程序。
2. 实现了不对称协程(asymmetric coroutine)的调度器。
3. 实现了协程任务的基本框架。
4. 使用rustsbi和uCore的一些基础库，目前测试程序正确运行在qemu模拟器下。

#### 问题

1. 协程思想中的生成器(generator)，在结构较为简单时，相比手动写的C风格生成器，性能差距较大（5-20倍）

   c风格生成器：

   ```c
   struct simple_generator{
     int a1=0;
     int a2=1;
     int generate(){
       int temp = a1 + a2;
       a1 = a2;
       a2 = temp;
       return a2;
     }
   }
   ```

   因为对于结构简单的C风格生成器来说，可以分配在栈上，且不太需要牵扯到调度器，暂停位置上下文简单。后续对于类似代码，如果追求性能，可能反而要避免使用协程写法的生成器。

   

### 第四周(10.3-10.9)

#### 进展

1. 正在迁移uCore-SMP的代码
2. 实现了对称协程(symmetric coroutine)，在开启编译优化尾递归的情况下可以避免调度器的参与。


### 第五周(10.10-10.16)

#### 进展

1. 修复了调度器内存泄漏的问题
2. 慢慢用C++的风格重写uCore-SMP的一些底层部分，比如spinlock, cpu等等
3. 实现了一个异步logger

#### 问题

1. 编译器的`Internal Compiler Error`

```
during GIMPLE pass: lower
os/utils/fprintf.cc: In function 'void __fprintf(__fprintf(file*, const char*)::_Z9__fprintfP4filePKc.Frame*)':
os/utils/fprintf.cc:4:12: internal compiler error: in lower_stmt, at gimple-low.cc:410
    4 | task<void> __fprintf(file* f, const char* fmt) {
      |            ^~~~~~~~~
0x611581 lower_stmt
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:410
0x611581 lower_sequence
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:217
0x13a6e8c lower_stmt
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:286
0x13a6e8c lower_sequence
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:217
0x13a6c68 lower_gimple_bind
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:475
0x13a6eec lower_stmt
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:255
0x13a6eec lower_sequence
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:217
0x13a6c68 lower_gimple_bind
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:475
0x13a6eec lower_stmt
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:255
0x13a6eec lower_sequence
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:217
0x13a6e8c lower_stmt
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:286
0x13a6e8c lower_sequence
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:217
0x13a6c68 lower_gimple_bind
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:475
0x13a79b8 lower_function_body
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:110
0x13a79b8 execute
        /tmp/rv_gcc/riscv-gnu-toolchain/gcc/gcc/gimple-low.cc:195
Please submit a full bug report, with preprocessed source (by using -freport-bug).
Please include the complete backtrace with any bug report.
See <https://gcc.gnu.org/bugs/> for instructions.
make: *** [Makefile:77: build/os/utils/fprintf.o] Error 1
```



#### 解决问题

1. 上面的ICE似乎是因为某个类里有一个析构函数，删了就好了。
2. 内存泄漏是因为协程task的所有权问题（可能属于调度器，或因为它是最上层task而没有人拥有它，需要自己执行完后自己释放自己。）情况比较复杂，最后直接在所有的协程入口包了一层。



#### 疑问

1. riscv有一个`mcpuid`/`mhartid`，但似乎supervisor层的实现都是在初始化时由m层来提供`hartid`使,`tp`存储`hartid`，sbi没有提供对应接口来访问`mcpuid`/`mhartid`



#### 下一步

1. 动态cpu分配
2. c++异步问题和对比



### 第六周(10.17-10.23)

#### 进展

1. 重写了`process`，现在分为`kernel_process`和`user_process`。`kernel`中只有在运行`kernel_process`时才打开时钟中断。（异步的系统调用足够短，可以忽略时钟中断，阻塞的系统调用会把进程设为睡眠状态，并切换到其他进程，在中间的处理过程也足够短。）既然足够短，无需为每一个进程分配一个内存栈，只需要为每个`cpu`分配一个共享的内核栈，在`user_trap`的时候使用

2. 目前的想法是父进程退出杀死所有子进程，但调度器可能拥有对子进程的指针，因此不清楚什么时候要释放资源。现在直接用了智能指针。

   

#### 问题

1. 现在还有一些`data race`






### 第七周(10.24-10.30)

#### 进展

1. 主要在修复问题
2. 研究了一下内核异常时gdb的`trace`，一个方案是为`kernelvec`补充`.cfi_*`的编译器指导语句。最后不好用，最后直接断在`sret`用`si`单步调试再`bt`

```assembly
			.cfi_startproc
        
        // set func2.cfa
        addi sp, sp, -16
      .cfi_def_cfa_offset 16
 
        // save func2.fp
        sd fp, 0(sp)
			.cfi_offset 8, -16

        // set up func2.ra
        csrr fp, sepc
        sd fp, 8(sp)
      .cfi_offset 1, -8
        // below here, cfi for func2 is valid


        // set up new fp and set func2.cfa
        addi fp, sp, 16
      .cfi_def_cfa 8, 0

        // now, ra(after call), fp, sp are all kernel_vec's

        // make room to save registers.
        addi sp, sp, -256
        // save the registers.
        sd ra, 0(sp)
        // ...
```



#### 问题
```c++
void cpu::push_off(){
    int old = intr_get();

    intr_off();
    if (noff == 0) {
        base_interrupt_status = old;
    }
    noff += 1;
}
```

实际上这两个操作不是原子的，可能会导致`intr_get()`的结果不正确。例如在获取完后被调度到另一个cpu上。
实际做这件事需要使用它的原子版本:`csrrc`,这里仿照了linux的做法

```c++
static int local_irq_save(){
    int old = rc_sstatus(SSTATUS_SIE);
    return old;
}

static void local_irq_restore(int old){
    s_sstatus(old & SSTATUS_SIE);
}
```

```c++
static inline uint64 rc_sstatus(uint64 val){
    unsigned long __v = (unsigned long)(val);		
	__asm__ __volatile__ ("csrrc %0, sstatus, %1"
			      : "=r" (__v) : "rK" (__v)		
			      : "memory");			
	return __v;	
}

static inline void s_sstatus(uint64 val){
    unsigned long __v = (unsigned long)(val);		
	__asm__ __volatile__ ("csrs sstatus, %0"	
			      : : "rK" (__v)			
			      : "memory");	
}
```



### 第八周(10.31-11.6)

#### 进展

1. 异步磁盘驱动（`virtio_blk`）具体的实现方式是，有一个协程风格的`wait_queue`，在上面`sleep`时，wait_queue把协程添加到一个链表中，并释放其所拥有的锁，协程不会重新被添加回调度器中，直到其被某个位置`wake_up`，把协程添加回调度器中，并在真正继续执行时重新获取锁。

```c++
    struct wait_queue_done {
        wait_queue* wq;
        task_base caller;
        spinlock& lock;

        bool await_ready() const { 
            return false; 
        }
        std::coroutine_handle<> await_suspend(task_base h) {
            promise_base* p = h.get_promise();
            if(p->no_yield) {
                return h.get_handle(); // resume immediately
            }

            caller = std::move(h);

            wq->sleep(&caller);
            lock.unlock();

            // switch back to scheduler
            return std::noop_coroutine();
        }
        void await_resume() {
            lock.lock();
        }
    };
    private:
    list<sleepable*> sleepers;

```

2. 因此在进行磁盘操作时，在某队列上休眠，中断到来时唤醒

```c++
    // Wait for virtio_disk_intr() to say request has finished.
    while (!info[idx[0]].done) {
        ...
        co_await info[idx[0]].wait_queue.done(lock);
    }

```

```c++
task<void> virtio_disk::disk_rw_done(int id) {
    auto guard = make_lock_guard(lock);
    info[id].done = true;
    info[id].wait_queue.wake_up();
    co_return task_ok;
}
```

3. 在上面实现了一层`block_buffer`，也有一个`request_queue`，当被使用时，休眠直到被使用完。

```c++
		task<uint8*> __get() {
        lock.lock();
        while (in_use) {
            co_await queue.done(lock);
        } 

        in_use = true;

        lock.unlock();
        
        co_await read_from_device();

        co_return data;
    }
```

### 第九周(11.7-11.13)

#### 进展

1. 异步文件系统雏形，一些简单的异步文件读写操作。

2. 检测未使用的协程



### 第十周(11.14-11.20)

#### 进展

1. 异步文件系统雏形,`dentry cache`
2. 注意到`inode cache`,`dentry cache`,`block device buffer`十分类似，抽象了一层`buffer`出来统一处理引用计数，休眠锁的逻辑。


#### 疑问

1. qemu上缓存和DMA一致性的问题