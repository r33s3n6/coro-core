# core-core 开发日志

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
