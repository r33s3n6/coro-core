#include <utils/log.h>
#include <utils/assert.h>
#include <proc/process.h>

#include <device/block/buf.h>

int test=0;
int test2=0;

#define BIG_NUMBER (1<<22)

spinlock lock;

void print_something(){
    debug_core("print_something");
    for (int j=0;j<1;j++){
        for (uint64 i=0;i< BIG_NUMBER;i++){
            lock.lock();
            int temp = test++;
            lock.unlock();
            if(temp % (BIG_NUMBER) == 0){

                cpu_ref c = cpu::my_cpu();
                int task_id = c->get_kernel_process()->get_pid();
                int id = c->get_core_id();
                debug_core("task: %d, core %d: %d", task_id, id, temp);
            }
        }
    }
    debug_core("print_something: end");
    
}

void test_bind_core(void* arg){

    uint64 __expect_core = *(int*)arg;

    for (uint64 i=0;i< BIG_NUMBER;i++){
        lock.lock();
        int temp = test2++;
        lock.unlock();
        if(temp % (BIG_NUMBER) == 0){

            cpu_ref c = cpu::my_cpu();
            int task_id = c->get_kernel_process()->get_pid();
            uint64 __id = c->get_core_id();
            debug_core("test_bind_core: pid:%d,expect core:%d core %d: %d", task_id, __expect_core, __id, temp);
            if (__id != __expect_core){
                debug_core("test_bind_core failed: pid: %d, expect core: %d, real_core: %d", task_id, __expect_core, __id);
                kernel_assert(false, "");
            }
        }
    }

    cpu_ref c = cpu::my_cpu();
    int task_id = c->get_kernel_process()->get_pid();
    uint64 __id = c->get_core_id();
    debug_core("test_bind_core: pid:%d,expect core:%d core %d", task_id, __expect_core, __id);
}

task<void> test_disk_read(int block_no, int len) {
    debugf("test_disk_read: block_no: %d, len: %d", block_no, len);
    auto buffer_ref = co_await kernel_block_buffer.get_node(virtio_disk_id, block_no);
    auto& buffer = *buffer_ref;
    std::optional<const uint8*> data_ptr = co_await buffer.get_for_read();
    if(!data_ptr) {
        co_return task_fail;
    }
    const uint8* data = *data_ptr;
    debug_core("test_disk_read: block_no: %d, len: %d, data: %s", block_no, len, data);

    // no need to put buffer, it will be put automatically
    co_return task_ok;
}

task<void> test_disk_write(int block_no, uint8* buf, int len, int id) {
    debugf("test_disk_write: block_no: %d, len: %d", block_no, len);
    
    // get buffer reference
    auto buffer_ref = co_await kernel_block_buffer.get_node(virtio_disk_id, block_no);
    auto& buffer = *buffer_ref;

    // try to get buffer for write
    std::optional<uint8*> data_ptr = co_await buffer.get_for_write();
    if(!data_ptr) {
        co_return task_fail;
    }
    uint8* data = *data_ptr;

    char old_bytes[128];

    debug_core("test_disk_write: block_no: %d, len: %d", block_no, len);
    for(int i = 0; i < len + 1; i++) {
        old_bytes[i] = data[i];
        data[i] = buf[i];
    }
    data[len] = '0' + id;
    old_bytes[len+1] = 0;
    debug_core("test_disk_write: block_no: %d, len: %d, old_bytes: %s", block_no, len, old_bytes);

    // buffer.put();

    // co_await buffer.flush();

    // co_await test_disk_read(block_no, len);

    co_return task_ok;
}

task<void> test_buffer_reuse() {
    debug_core("test_buffer_reuse");

    for(int i = 0; i < 10; i++) {
        auto buffer_ref = co_await kernel_block_buffer.get_node(virtio_disk_id, 20+i);
        auto& buffer = *buffer_ref;

        // try to get buffer for write
        std::optional<uint8*> data_ptr = co_await buffer.get_for_write();
        if(!data_ptr) {
            co_return task_fail;
        }
        uint8* data = *data_ptr;

        data[0] = '0' + i;
        data[1] = 0;
        debug_core("test_buffer_reuse: block_no: %d, data: %s, &data: %p", 20+i, data, (void*)data);
    }
    co_return task_ok;
}

void test_disk_rw(void*){
    debugf("test_disk_rw");
    for (int i = 0; i < 10;i++) {
       kernel_task_queue.push(test_disk_write(0, (uint8*)"hello\0", 5, i));
       kernel_task_queue.push(test_disk_read(0, 10));
    }

    for (int i = 0; i < 10;i++) {
       kernel_task_queue.push(test_disk_write(i+1, (uint8*)"hello\0", 5, i));
       kernel_task_queue.push(test_disk_read(i+1, 10));
    }
    
    kernel_task_queue.push(test_buffer_reuse());

    
}