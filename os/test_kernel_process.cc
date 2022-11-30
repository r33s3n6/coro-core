#include <utils/log.h>
#include <utils/assert.h>
#include <proc/process.h>

#include <device/block/buf.h>

#include <fs/nfs/nfs.h>
#include <fs/nfs/inode.h>

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
/*
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

    // ~buffer() call buffer.put() automatically

    // co_await buffer.flush();

    // co_await test_disk_read(block_no, len);

    co_return task_ok;
}

task<void> test_buffer_reuse() {
    debug_core("test_buffer_reuse");

    for(int i = 0; i < 1000; i++) {
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
*/

static uint64 random_seed = 1423;
uint64 random() {
    random_seed = random_seed * 1103515245 + 12345;
    return random_seed;
}

static uint64 random_store[1024];

task<void> test_coherence(device_id_t test_device_id) {
    debug_core("test_coherence");

    block_device* test_device = device::get<block_device>(test_device_id);

    if (test_device->capacity() < 1024) {
        debug_core("test_coherence: device %p: not enough space: %d", test_device, test_device->capacity());
        co_return task_fail;
    }

    for(int i = 0; i < 1024; i++) {
        auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, i);
        auto buffer_ref = *co_await buffer_ptr->get_ref();
        
        // try to get buffer for write
        uint8* buf = buffer_ref->data;
        random_store[i] = random();
        ((uint64*)buf)[0] = random_store[i];
        buffer_ref->mark_dirty();
    }

    // check coherence 1

    for(int i = 0; i < 1024; i++) {
        auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, i);
        auto buffer_ref = *co_await buffer_ptr->get_ref();
        
        // try to get buffer for read
        const uint8* buf = buffer_ref->data;
        uint64 val = ((uint64*)buf)[0];
        if(val != random_store[i]) {
            debug_core("test_coherence: coherence 1 failed: i: %d, val: %p, random_store[i]: %p", i, (void*)val, (void*)random_store[i]);
            // kernel_assert(false, "");
            // break;
        }
    }

    co_await kernel_block_buffer.destroy(test_device_id);

    // check coherence 2

    for(int i = 0; i < 1024; i++) {
        auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, i);
        auto buffer_ref = *co_await buffer_ptr->get_ref();
        
        // try to get buffer for read
        const uint8* buf = buffer_ref->data;
        uint64 val = ((uint64*)buf)[0];
        if(val != random_store[i]) {
            debug_core("test_coherence: coherence 2 failed: i: %d, val: %p, random_store[i]: %p", i, (void*)val, (void*)random_store[i]);
            // break;
            // kernel_assert(false, "");
        }
    }

    debug_core("test_coherence: ok");

    co_return task_ok;

}


class test_bdev_coherence  {
    single_wait_queue _wait_queue;
    uint32 _subtask_complete;
    spinlock lock;
    bool _test_ok = false;

    task<void> subtask_write(device_id_t test_device_id, uint32 block_no, uint64* random_store_ptr) {
        block_device* test_device = device::get<block_device>(test_device_id);

        uint64 random_store = random();
        {
            auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, block_no);
            auto buffer_ref = *co_await buffer_ptr->get_ref();
            // try to get buffer for write
            uint8* buf = buffer_ref->data;
            ((uint64*)buf)[0] = random_store;
            buffer_ref->mark_dirty();
        }

        *random_store_ptr = random_store;

        lock.lock();
        _subtask_complete++;
        if(_subtask_complete == 1024) {
            _wait_queue.wake_up();
        }
        lock.unlock();

        co_return task_ok;
    }

    task<void> subtask_read(device_id_t test_device_id, uint32 block_no, uint64 random_store, int index, int pass) {
        block_device* test_device = device::get<block_device>(test_device_id);
        {
            auto buffer_ptr = *co_await kernel_block_buffer.get(test_device, block_no);
            auto buffer_ref = *co_await buffer_ptr->get_ref();
            // try to get buffer for read
            const uint8* buf = buffer_ref->data;

            uint64 val = ((uint64*)buf)[0];
            if(val != random_store) {
                debug_core("test_coherence 2: coherence %d failed: index: %d, val: %p, expect: %p", pass, index, (void*)val, (void*)random_store);
            }

        }

        lock.lock();
        _subtask_complete++;
        if(_subtask_complete == 1024) {
            _wait_queue.wake_up();
        }
        lock.unlock();

        co_return task_ok;
    }

public:
    test_bdev_coherence() {}
    task<void> get_test(device_id_t test_device_id) {
        debug_core("test_coherence");

        block_device* test_device = device::get<block_device>(test_device_id);

        if (test_device->capacity() < 1024) {
            debug_core("test_coherence: device %p: not enough space: %d", test_device, test_device->capacity());
            co_return task_fail;
        }

        _subtask_complete = 0;

        for(int i = 0; i < 1024; i++) {
            kernel_task_queue.push(subtask_write(test_device_id, i, &random_store[i]));
        }

        lock.lock();
        while(_subtask_complete < 1024) {
            co_await _wait_queue.done(lock);
        }
        lock.unlock();


        _subtask_complete = 0;
        // check coherence 1

        for(int i = 0; i < 1024; i++) {
            kernel_task_queue.push(subtask_read(test_device_id, i, random_store[i], i, 1));
        }

        lock.lock();
        while(_subtask_complete < 1024) {
            co_await _wait_queue.done(lock);
        }
        lock.unlock();


        _subtask_complete = 0;

        co_await kernel_block_buffer.destroy(test_device_id);

        // check coherence 2
        for(int i = 0; i < 1024; i++) {
            kernel_task_queue.push(subtask_read(test_device_id, i, random_store[i], i, 2));
        }

        lock.lock();
        while(_subtask_complete < 1024) {
            co_await _wait_queue.done(lock);
        }
        _test_ok = true;
        lock.unlock();


        debug_core("test_coherence: ok");

        co_return task_ok;

    }

    bool is_success() {
        auto guard = make_lock_guard(lock);
        return _test_ok;
    }


};






void test_disk_rw(void*){
    debugf("test_disk_rw");

    test_bdev_coherence test;

    // auto task = test_coherence2(virtio_disk_id);

    kernel_task_queue.push(test.get_test(virtio_disk_id));

    while(!test.is_success()) {
        cpu::my_cpu()->yield();
    }

    // for (int i = 0; i < 10;i++) {
    //    kernel_task_queue.push(test_disk_write(0, (uint8*)"hello\0", 5, i));
    //    kernel_task_queue.push(test_disk_read(0, 10));
    // }
// 
    // for (int i = 0; i < 10;i++) {
    //    kernel_task_queue.push(test_disk_write(i+1, (uint8*)"hello\0", 5, i));
    //    kernel_task_queue.push(test_disk_read(i+1, 10));
    // }
    // 
    // kernel_task_queue.push(test_buffer_reuse());

    
}


task<void> test_nfs_read(device_id_t device_id, uint32 times) {
    debugf("test_nfs_read");
    
    nfs::nfs test_fs;
    co_await test_fs.mount(device_id);
    test_fs.print();

    // find file using path
    dentry* file_dentry = *co_await kernel_dentry_cache.get_at(nullptr, "/test_file");
    inode* file_inode = file_dentry->get_inode();
    file_inode->get();
    kernel_dentry_cache.put(file_dentry);

    // test file rw
    simple_file _file((inode*)file_inode);
    co_await file_inode->put();
    co_await _file.open();
    int64 file_size = *co_await _file.llseek(0, file::whence_t::END);

    debugf("test_make_nfs: file_size: %d", file_size);

    
    co_await _file.llseek(0, file::whence_t::SET);


    // check
    for (uint32 i = 0; i < times; i++) {
        char buf[48];
        const char* expect = "hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0";
        int64 read_size = *co_await _file.read(buf, 48);
        if (read_size != 48) {
            debugf("test_make_nfs: read_size: %d", read_size);
            kernel_assert(false, "");
        }
        for (int j = 0; j < 48; j++) {
            if (buf[j] != expect[j]) {
                debugf("test_make_nfs: buf[%d]: %d, expect[%d]: %d", j, buf[j], j, expect[j]);
                kernel_assert(false, "");
            }
        }
        
    }

    co_await _file.close();

    debugf("test_nfs_read: read ok");

    // delete file
    file_dentry = *co_await kernel_dentry_cache.get_at(nullptr, "/test_file");
    dentry* parent_dentry = file_dentry->parent;
    co_await parent_dentry->get_inode()->unlink(file_dentry);
    kernel_dentry_cache.put(file_dentry);

    co_await test_fs.unmount();
    debugf("test_nfs_read: ok");

    co_return task_ok;

}

task<void> test_make_nfs(device_id_t device_id, uint32 times) {
    debugf("test_make_nfs");

    // make fs
    co_await nfs::nfs::make_fs(device_id, 8192);
    debugf("test_make_nfs: make_fs done");

    // mount fs
    nfs::nfs test_fs;

    co_await test_fs.mount(device_id);
    test_fs.print();

    // get root and check metadata
    inode* root_inode = *co_await test_fs.get_root();
    kernel_assert(root_inode, "test_make_nfs: root_inode is null");
    auto metadata1 = *co_await root_inode->get_metadata();
    kernel_assert(metadata1->type == inode::ITYPE_DIR, "metadata.type != T_DIR");

    dentry* root_dentry = *co_await root_inode->get_dentry();
    kernel_assert(root_dentry, "test_make_nfs: root_dentry is null");

    // create simple file
    dentry* file_dentry = *co_await kernel_dentry_cache.create(root_dentry, "test_file", nullptr);
    kernel_assert(file_dentry, "test_make_nfs: file_dentry is null");
    debugf("test_make_nfs: create file_dentry done");
    co_await root_inode->create(file_dentry);
    debugf("test_make_nfs: create file done");
    inode* file_inode = file_dentry->get_inode();
    kernel_assert(file_inode, "test_make_nfs: file_inode is null");
    file_inode->get();
    kernel_dentry_cache.put(file_dentry);

    debugf("file_inode ref: %d", file_inode->get_ref());

    int64 write_size = *co_await file_inode->write("hello world", 0, 12);
    if (write_size != 12) {
        warnf("nfs: write failed:%d", write_size);
        co_return task_fail;
    }
    debugf("nfs: write file inode %d done", file_inode->inode_number);
    co_await file_inode->put();

    debugf("file_inode ref: %d", file_inode->get_ref());

    // find file using path
    dentry* file_dentry2 = *co_await kernel_dentry_cache.get_at(nullptr, "/test_file");
    file_inode = file_dentry2->get_inode();
    file_inode->get();
    kernel_dentry_cache.put(file_dentry2);

    debugf("file_inode ref: %d", file_inode->get_ref());

    auto metadata2 = *co_await file_inode->get_metadata();
    kernel_assert(metadata2->type == inode::ITYPE_FILE, "metadata.type != T_FILE");

    // test file rw
    simple_file _file((inode*)file_inode);
    co_await _file.open();
    co_await file_inode->put();

    int64 file_size = *co_await _file.llseek(0, file::whence_t::END);

    debugf("test_make_nfs: file_size: %d", file_size);
    char* buf = new char[file_size + 1];
    buf[file_size] = 0;
    co_await _file.llseek(0, file::whence_t::SET);
    int64 read_size = *co_await _file.read(buf, file_size);

    debugf("test_make_nfs: read_size: %d", read_size);
    debugf("test_make_nfs: buf: %s", buf);

    // test bulk write
    co_await _file.llseek(0, file::whence_t::SET);
    for (uint32 i = 0; i < times; i++) {
        co_await _file.write("hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0", 48);

        if (i % 1024 == 0) {
            debugf("test_make_nfs: write %d", i);
        }
    }

    debugf("test_make_nfs: write done");

    int64 new_file_size = *co_await _file.llseek(0, file::whence_t::END);
    co_await _file.llseek(0, file::whence_t::SET);
    debugf("test_make_nfs: new_file_size: %d", new_file_size);

    for (uint32 i = 0; i < times; i++) {
        char buf[48];
        const char* expect = "hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0hi\0";
        int64 read_size = *co_await _file.read(buf, 48);
        if (read_size != 48) {
            debugf("test_make_nfs: read_size: %d", read_size);
            kernel_assert(false, "");
        }
        for (int j = 0; j < 48; j++) {
            if (buf[j] != expect[j]) {
                debugf("test_make_nfs: i:%d buf[%d]: %d, expect[%d]: %d",i,  j, buf[j], j, expect[j]);
                // kernel_assert(false, "");
            }
        }
        
    }

    co_await _file.close();

    // file_inode->print();
    // ((nfs::nfs_inode*)root)->print();
// 
    // co_await test_fs.put_inode(file_inode);

    // print superblock

    //test_fs.print();
    co_await test_fs.unmount();

    co_await test_nfs_read(device_id, times);

    infof("inode inner cache: %d/%d", nfs::nfs_inode::cache_hit, nfs::nfs_inode::cache_miss);

    co_return task_ok;

}

task<void> test_make_nfs2(device_id_t device_id) {
    debugf("test_make_nfs");


    co_await nfs::nfs::make_fs(device_id, 8192);
    debugf("test_make_nfs: make_fs done");

    nfs::nfs test_fs;

    co_await test_fs.mount(device_id);
    test_fs.print();
    inode* root = *co_await test_fs.get_root();
    auto metadata1 = *co_await root->get_metadata();
    kernel_assert(metadata1->type == inode::ITYPE_DIR, "metadata.type != T_DIR");

    nfs::dirent first_file;
    int64 read_size = *co_await root->read(&first_file, 0, sizeof(nfs::dirent));
    kernel_assert(read_size == sizeof(nfs::dirent), "read_size != sizeof(nfs::dirent)");

    debugf("test_make_nfs: first_file: %d: %s",first_file.inode_number, first_file.name);
    nfs::nfs_inode* file_inode = *co_await test_fs.get_inode(first_file.inode_number);

    auto metadata2 = *co_await file_inode->get_metadata();
    kernel_assert(metadata2->type == inode::ITYPE_FILE, "metadata.type != T_FILE");

    // test file rw
    simple_file _file((inode*)file_inode);
    co_await _file.open();
    int64 file_size = *co_await _file.llseek(0, file::whence_t::END);

    debugf("test_make_nfs: file_size: %d", file_size);
    char* buf = new char[file_size + 1];
    buf[file_size] = 0;
    co_await _file.llseek(0, file::whence_t::SET);
    read_size = *co_await _file.read(buf, file_size);

    debugf("test_make_nfs: read_size: %d", read_size);
    debugf("test_make_nfs: buf: %s", buf);
    delete buf;

    // test bulk write
    co_await _file.llseek(0, file::whence_t::SET);
    co_await _file.write(skernel, ekernel-skernel);


    debugf("test_make_nfs: write done");

    int64 new_file_size = *co_await _file.llseek(0, file::whence_t::END);
    co_await _file.llseek(0, file::whence_t::SET);
    debugf("test_make_nfs: new_file_size: %d", new_file_size);

    buf = new char[new_file_size + 1];
    buf[new_file_size] = 0;

    read_size = *co_await _file.read(buf, new_file_size);
    debugf("test_make_nfs: read_size: %d", read_size);


    for (uint32 i = 0; i < ekernel-skernel; i++) {
        if (buf[i] != skernel[i]) {
            debugf("test_make_nfs: i:%d buf[%d]: %d, skernel[%d]: %d",i,  i, buf[i], i, skernel[i]);
            // kernel_assert(false, "");
        }
        
    }

    delete[] buf;

    co_await _file.close();

    file_inode->print();
    ((nfs::nfs_inode*)root)->print();

    co_await test_fs.put_inode(file_inode);

    // print superblock
    debugf("test_fs.sb.used_generic_blocks: %d", test_fs.sb.used_generic_blocks);

    //test_fs.print();
    co_await test_fs.unmount();

    infof("inode inner cache: %d/%d", nfs::nfs_inode::cache_hit, nfs::nfs_inode::cache_miss);

    co_return task_ok;

}

void test_nfs(void*){
    
    // kernel_task_queue.push(test_make_nfs(ramdisk_id, 64 * 1024));
    kernel_task_queue.push(test_make_nfs(virtio_disk_id, 64 * 1024));

    // kernel_task_queue.push(test_make_nfs2(virtio_disk_id));
}