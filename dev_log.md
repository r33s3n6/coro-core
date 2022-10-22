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

1. 重写了`process`，现在分为`kernel_process`和`user_process`。`kernel`中只有在运行`kernel_process`时才打开时钟中断。

