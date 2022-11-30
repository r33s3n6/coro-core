#include <utils/log.h>
#include <utils/assert.h>
#include <proc/process.h>

#include <device/block/buf.h>

#include <fs/nfs/nfs.h>
#include <fs/nfs/inode.h>

task<void> test_nfs2_coro(device_id_t device_id) {
    nfs::nfs test_fs2;
    co_await test_fs2.mount(device_id);

    dentry* file_dentry3_2 = *co_await kernel_dentry_cache.get_at(nullptr, "/test_dir/test_file3");
    
    inode * file_inode = file_dentry3_2->get_inode();
    {
        simple_file _file3(file_inode);
        co_await _file3.open();
        co_await _file3.llseek(0, file::whence_t::SET);
        char buf2[13];
        buf2[12] = 0;
        int64 read_size2 = *co_await _file3.read(buf2, 12);
        co_await _file3.close();

        kernel_assert(read_size2 == 12, "test_make_nfs: read_size2 != 12");
        debugf("test_make_nfs: buf2: %s", buf2);
    }

    dentry* root = *co_await kernel_dentry_cache.get_at(file_dentry3_2, "../../");
    debugf("test_make_nfs: root: '%s' %d", root->name.data(), root->get_inode()->inode_number);

    debugf("iterate root dir...");
    // read dir
    {
        auto dir_iterator = root->get_inode()->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_make_nfs: read dir: %s %d", dentry->name.data(), dentry->get_inode()->inode_number);
            count++;
        }
    
    }

    co_await test_fs2.unmount();

}

task<void> test_nfs_coro(device_id_t device_id) {
    debugf("test_nfs");

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

    // debugf("test_make_nfs: file_size: %d", file_size);
    kernel_assert(file_size == 12, "test_make_nfs: file_size != 12");
    char buf[13];
    buf[12] = 0;
    co_await _file.llseek(0, file::whence_t::SET);
    int64 read_size = *co_await _file.read(buf, file_size);

    debugf("test_make_nfs: read_size: %d", read_size);
    debugf("test_make_nfs: buf: %s", buf);
    co_await _file.close();

    debugf("file closed");

    // create dir
    dentry* dir_dentry = *co_await kernel_dentry_cache.create(root_dentry, "test_dir", nullptr);
    debugf("dir_dentry created");
    kernel_assert(dir_dentry, "test_make_nfs: dir_dentry is null");

    co_await root_inode->mkdir(dir_dentry);
    debugf("test_make_nfs: create dir done");

    inode* dir_inode = dir_dentry->get_inode();
    kernel_assert(dir_inode, "test_make_nfs: dir_inode is null");

    dir_inode->get();
    
    // create 2 files
    dentry* file_dentry3 = *co_await kernel_dentry_cache.create(dir_dentry, "test_file3", nullptr);
    dentry* file_dentry4 = *co_await kernel_dentry_cache.create(dir_dentry, "test_file4", nullptr);

    co_await dir_inode->create(file_dentry3);
    co_await dir_inode->create(file_dentry4);

    debugf("test_make_nfs: file3_ref: %d", file_dentry3->get_inode()->get_ref());
    debugf("test_make_nfs: file4_ref: %d", file_dentry4->get_inode()->get_ref());

    debugf("test_make_nfs: create 2 files done");

    // read dir
    {
        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_make_nfs: read dir: %s %d", dentry->name.data(), dentry->get_inode()->inode_number);
            count++;
        }
    
        kernel_assert(count == 2, "test_make_nfs: count != 2");
    }

    debugf("test_make_nfs: file3_ref: %d", file_dentry3->get_inode()->get_ref());
    debugf("test_make_nfs: file4_ref: %d", file_dentry4->get_inode()->get_ref());


    // link file
    dentry* file_dentry_link = *co_await kernel_dentry_cache.create(dir_dentry, "test_file3_link", nullptr);
    co_await dir_inode->link(file_dentry3, file_dentry_link);


    debugf("test_make_nfs: link file done");

    debugf("test_make_nfs: file3_ref: %d", file_dentry3->get_inode()->get_ref());
    debugf("test_make_nfs: file4_ref: %d", file_dentry4->get_inode()->get_ref());
    
    // read dir
    {
        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_make_nfs: read dir: %s %d", dentry->name.data(), dentry->get_inode()->inode_number);
            count++;
        }
    
        kernel_assert(count == 3, "test_make_nfs: count != 3");
    }

    debugf("test_make_nfs: file3_ref: %d", file_dentry3->get_inode()->get_ref());
    debugf("test_make_nfs: file4_ref: %d", file_dentry4->get_inode()->get_ref());

    // test write linked file
    {
        simple_file _file2((inode*)file_dentry_link->get_inode());

        co_await _file2.open();
        co_await _file2.llseek(0, file::whence_t::SET);
        int64 write_size2 = *co_await _file2.write("hello world", 12);
        co_await _file2.close();

        kernel_assert(write_size2 == 12, "test_make_nfs: write_size2 != 12");
    }


    // test read linked file
    {
        simple_file _file3(file_dentry3->get_inode());
        co_await _file3.open();
        co_await _file3.llseek(0, file::whence_t::SET);
        char buf2[13];
        buf2[12] = 0;
        int64 read_size2 = *co_await _file3.read(buf2, 12);
        co_await _file3.close();

        kernel_assert(read_size2 == 12, "test_make_nfs: read_size2 != 12");

        debugf("test_make_nfs: buf2: %s", buf2);
    }

    // test unlink file (delete file)
    {
        co_await dir_inode->unlink(file_dentry4);
        kernel_dentry_cache.put(file_dentry4);

        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_make_nfs: read dir: %s %d", dentry->name.data(), dentry->get_inode()->inode_number);
            count++;
        }
    
        kernel_assert(count == 2, "test_make_nfs: count != 2");

    }

    co_await dir_inode->put();

    kernel_dentry_cache.put(file_dentry3);
    kernel_dentry_cache.put(dir_dentry);

    co_await test_fs.unmount();

    debugf("test_make_nfs: done");

    infof("inode inner cache: %d/%d", nfs::nfs_inode::cache_hit, nfs::nfs_inode::cache_miss);

    co_await test_nfs2_coro(device_id);

    co_return task_ok;

}

void test_nfs2(void*){
 
    kernel_task_queue.push(test_nfs_coro(virtio_disk_id));

}