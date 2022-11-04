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
}

task<void> test_disk_read(int block_no, int len) {
    debugf("test_disk_read: block_no: %d, len: %d", block_no, len);
    std::optional<block_buffer_node_ref> buffer_ref = co_await kernel_block_buffer.get_node({VIRTIO_DISK_MAJOR, VIRTIO_DISK_MINOR}, block_no);
    auto buffer = *buffer_ref;
    std::optional<const uint8*> data_ptr = co_await buffer.get_for_read();
    if(!data_ptr) {
        co_return task_fail;
    }
    const uint8* data = *data_ptr;
    debug_core("test_disk_read: block_no: %d, len: %d", block_no, len);
    for(int i = 0; i < len; i++) {
        debug_core("byte %d: %x", i, data[i]);
    }
    // no need to put buffer, it will be put automatically
    co_return task_ok;
}

task<void> test_disk_write(int block_no, uint8* buf, int len) {
    debugf("test_disk_write: block_no: %d, len: %d", block_no, len);
    std::optional<block_buffer_node_ref> buffer_ref = co_await kernel_block_buffer.get_node({VIRTIO_DISK_MAJOR, VIRTIO_DISK_MINOR}, block_no);
    auto buffer = *buffer_ref;
    std::optional<uint8*> data_ptr = co_await buffer.get_for_write();
    if(!data_ptr) {
        co_return task_fail;
    }
    uint8* data = *data_ptr;
    debug_core("test_disk_write: block_no: %d, len: %d", block_no, len);
    for(int i = 0; i < len; i++) {
        debug_core("old byte %d: %x", i, data[i]);
        data[i] = buf[i];
    }
    buffer.put();

    co_await buffer.flush();

    co_await test_disk_read(block_no, len);

    co_return task_ok;
}

void test_disk_rw(void*){
    debugf("test_disk_rw");
    kernel_task_queue.push(test_disk_write(0, (uint8*)"hello", 5));
    kernel_task_queue.push(test_disk_write(1, (uint8*)"world", 5));

    
}