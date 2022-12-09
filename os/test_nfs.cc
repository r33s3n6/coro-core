#include <proc/process.h>
#include <utils/assert.h>
#include <utils/log.h>

#include <device/block/buf.h>

#include <fs/nfs/inode.h>
#include <fs/nfs/nfs.h>

#include <mm/allocator.h>

#include <fs/inode_cache.h>

#include <arch/cpu.h>

#include <task_scheduler.h>

uint64 a = 0;

task<void> test_coro2(device_id_t) {
    debugf("test_coro2: start");
    // panic("test panic");
    // uint64* ptr = (uint64*)0x100uLL;
    // uint64 val = *ptr;
    // val = val;
    co_return task_ok;
}

task<void> test_coro(void* arg) {
    debugf("test_nfs");

    {
        debugf("call test_coro2");
        co_await test_coro2({});
        debugf("test_coro2: done");
    }

    volatile uint64* ptr = (volatile uint64*)arg;
    for (int i = 0; i < 100000; i++) {
        *ptr = !*ptr;
    }

    co_return task_ok;
}

task<void> test_nfs2_coro(device_id_t device_id) {
    nfs::nfs test_fs2;

    // (void)(test_fs2);
    co_await test_fs2.mount(device_id);
    {
        shared_ptr<dentry> file_dentry3_2 =
            *co_await kernel_dentry_cache.get_at(nullptr, "/test_dir/test_file3");

        shared_ptr<inode> file_inode = file_dentry3_2->get_inode();

        {
            simple_file _file3(file_inode);
            co_await _file3.open();
            co_await _file3.llseek(0, file::whence_t::SET);
            char buf2[13];
            buf2[12] = 0;
            int64 read_size2 = *co_await _file3.read(buf2, 12);
            co_await _file3.close();

            kernel_assert(read_size2 == 12, "test_nfs_coro: read_size2 != 12");
            debugf("test_nfs_coro: buf2: %s", buf2);
        }

        shared_ptr<dentry> root =
            *co_await kernel_dentry_cache.get_at(file_dentry3_2, "../../");
        debugf("test_nfs_coro: root: '%s' %d", root->name.data(),
               root->get_inode()->inode_number);

        for (int i=0;i<2;i++) {
            debugf("iterate root dir...");
            // read dir
            {
                auto dir_iterator = root->get_inode()->read_dir();

                int count = 0;
                while (auto dentry = *co_await dir_iterator) {
                    debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                           dentry->get_inode()->inode_number);
                    count++;
                }
            }
        }


    }

    co_await test_fs2.unmount();

    co_return task_ok;
}

task<shared_ptr<uint32>> test_generator(uint32 start) {
    
    for (int i=0;i<10;i++) {
        shared_ptr<uint32> ptr = nullptr;
        ptr = make_shared<uint32>(start + i);
        co_yield ptr;
    }
    co_yield nullptr;
}

task<void> test_nfs_coro_unit(device_id_t device_id) {
    debugf("test_nfs");

    co_await test_coro(&a);

    // make fs
    co_await nfs::nfs::make_fs(device_id, 8192);
    debugf("test_nfs_coro: make_fs done");

    // mount fs
    nfs::nfs test_fs;

    co_await test_fs.mount(device_id);
    test_fs.print();
    {
        // get root and check metadata
        shared_ptr<nfs::nfs_inode> root_inode = *co_await test_fs.get_root();
        kernel_assert(root_inode, "test_nfs_coro: root_inode is null");
        debugf("test_nfs_coro: get root done: %d", root_inode->inode_number);
        {
            reference_guard<nfs::nfs_inode> root_inode_ref =
                *co_await root_inode->get_ref();
            debugf("test_nfs_coro: root_inode: %d",
                   root_inode_ref->inode_number);
            auto metadata1 = *co_await root_inode_ref->get_metadata();
            kernel_assert(metadata1->type == inode::ITYPE_DIR,
                          "metadata.type != T_DIR");
            root_inode_ref->print();
        }

        shared_ptr<dentry> root_dentry = root_inode->get_dentry().lock();
        kernel_assert(root_dentry, "test_nfs_coro: root_dentry is null");

        // create simple file
        shared_ptr<dentry> file_dentry = *co_await kernel_dentry_cache.create(
            root_dentry, "test_file", nullptr);
        kernel_assert(file_dentry, "test_nfs_coro: file_dentry is null");
        debugf("test_nfs_coro: create file_dentry done");

        {
            debugf("test_nfs_coro: create file...");
            reference_guard<nfs::nfs_inode> root_inode_ref =
                *co_await root_inode->get_ref();
            root_inode_ref->print();
            co_await root_inode_ref->create(file_dentry);
            debugf("test_nfs_coro: create file done");
        }

        shared_ptr<inode> file_inode = file_dentry->get_inode();
        kernel_assert(file_inode, "test_nfs_coro: file_inode is null");

        {
            auto file_inode_ref = *co_await file_inode->get_ref();
            // kernel_dentry_cache.put(file_dentry);

            int64 write_size =
                *co_await file_inode_ref->write("hello world", 0, 12);
            if (write_size != 12) {
                warnf("nfs: write failed:%d", write_size);
                co_return task_fail;
            }
            debugf("nfs: write file inode %d done",
                   file_inode_ref->inode_number);
        }

        // find file using path
        shared_ptr<dentry> file_dentry2 =
            *co_await kernel_dentry_cache.get_at(nullptr, "/test_file");
        file_inode = file_dentry2->get_inode();
        {
            auto file_inode_ref = *co_await file_inode->get_ref();
            // kernel_dentry_cache.put(file_dentry2);

            auto metadata2 = *co_await file_inode_ref->get_metadata();
            kernel_assert(metadata2->type == inode::ITYPE_FILE,
                          "metadata.type != T_FILE");
        }

        {
            // test file rw
            simple_file _file(file_inode);
            co_await _file.open();
            file_inode.reset();

            int64 file_size = *co_await _file.llseek(0, file::whence_t::END);

            // debugf("test_nfs_coro: file_size: %d", file_size);
            kernel_assert(file_size == 12, "test_nfs_coro: file_size != 12");
            char buf[13];
            buf[12] = 0;
            co_await _file.llseek(0, file::whence_t::SET);
            int64 read_size = *co_await _file.read(buf, file_size);

            debugf("test_nfs_coro: read_size: %d", read_size);
            debugf("test_nfs_coro: buf: %s", buf);
            co_await _file.close();
            debugf("file closed");
        }

        // create dir
        shared_ptr<dentry> dir_dentry = *co_await kernel_dentry_cache.create(
            root_dentry, "test_dir", nullptr);
        debugf("dir_dentry created");
        kernel_assert(dir_dentry, "test_nfs_coro: dir_dentry is null");

        {
            auto root_inode_ref = *co_await root_inode->get_ref();

            co_await root_inode_ref->mkdir(dir_dentry);
            debugf("test_nfs_coro: create dir done");
        }

        shared_ptr<inode> dir_inode = dir_dentry->get_inode();
        kernel_assert(dir_inode, "test_nfs_coro: dir_inode is null");

        co_await dir_inode->get();

        // create 2 files
        shared_ptr<dentry> file_dentry3 = *co_await kernel_dentry_cache.create(
            dir_dentry, "test_file3", nullptr);
        shared_ptr<dentry> file_dentry4 = *co_await kernel_dentry_cache.create(
            dir_dentry, "test_file4", nullptr);

        co_await dir_inode->create(file_dentry3);
        co_await dir_inode->create(file_dentry4);

        debugf("test_nfs_coro: create 2 files done");

        // =========

        // {
        //     auto test_iter = test_generator(5);
        //     int count =0;
        //     while (auto result = *co_await test_iter) {
        //         debugf("test_nfs_coro: test_iter: %d", *result);
        //         count++;
        //     }
        //     kernel_assert(count == 10, "test_nfs_coro: count != 5");
        // }

        // {
        //     auto test_iter = test_generator(5);
        //     int count =0;
        //     while (auto result = *co_await test_iter) {
        //         debugf("test_nfs_coro2: test_iter: %d", *result);
        //         count++;
        //     }
        //     kernel_assert(count == 10, "test_nfs_coro: count != 5");
        // }

        // read dir
        {
            auto dir_iterator = dir_inode->read_dir();

            int count = 0;
            while (auto dentry = *co_await dir_iterator) {
                auto inode = dentry->get_inode();
                debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                       inode->inode_number);
                count++;
            }

            kernel_assert(count == 2, "test_nfs_coro: count != 2");
        }

        // read dir2
        {
            auto dir_iterator = dir_inode->read_dir();

            int count = 0;
            while (auto dentry = *co_await dir_iterator) {
                auto inode = dentry->get_inode();
                debugf("test_nfs_coro: read dir2: %s %d", dentry->name.data(),
                       inode->inode_number);
                count++;
            }

            kernel_assert(count == 2, "test_nfs_coro: count != 2");
        }

        // link file
        shared_ptr<dentry> file_dentry_link =
            *co_await kernel_dentry_cache.create(dir_dentry, "test_file3_link",
                                                 nullptr);
        co_await dir_inode->link(file_dentry3, file_dentry_link);

        debugf("test_nfs_coro: link file done");

        // read dir
        {
            auto dir_iterator = dir_inode->read_dir();

            int count = 0;
            while (auto dentry = *co_await dir_iterator) {
                auto inode = dentry->get_inode();
                debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                       inode->inode_number);
                count++;
            }

            kernel_assert(count == 3, "test_nfs_coro: count != 3");
        }


        // test write linked file
        {
            simple_file _file2(file_dentry_link->get_inode());

            co_await _file2.open();
            co_await _file2.llseek(0, file::whence_t::SET);
            int64 write_size2 = *co_await _file2.write("hello world", 12);
            co_await _file2.close();

            kernel_assert(write_size2 == 12,
                          "test_nfs_coro: write_size2 != 12");
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

            kernel_assert(read_size2 == 12, "test_nfs_coro: read_size2 != 12");

            debugf("test_nfs_coro: buf2: %s", buf2);
        }

            // test unlink file (delete file)
        {
            co_await dir_inode->unlink(file_dentry4);

            auto dir_iterator = dir_inode->read_dir();
 
            int count = 0;
            while (auto dentry = *co_await dir_iterator) {
                auto inode = dentry->get_inode();
                debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                       inode->inode_number);
                count++;
            }

            kernel_assert(count == 2, "test_nfs_coro: count != 2");
        }

        dir_inode->put();

    }

    co_await test_fs.unmount();

    kernel_inode_cache.print();
    kernel_dentry_cache.print();
    kernel_block_buffer.print();

    debugf("test_nfs_coro: done");

    infof("inode inner cache: %d/%d", nfs::nfs_inode::cache_hit,
          nfs::nfs_inode::cache_miss);

    co_await test_nfs2_coro(device_id);
    co_await test_coro(&a);
    
    debugf("test_nfs: ok");

    co_return task_ok;
}

task<void> test_nfs_coro(device_id_t device_id) {
    for (int i = 0; i < 1; i++) {
        co_await test_nfs_coro_unit(device_id);
    }
}

task<void> __test_nfs_coro(device_id_t device_id) {
    debugf("test_nfs");

    // make fs
    co_await nfs::nfs::make_fs(device_id, 8192);
    debugf("test_nfs_coro: make_fs done");

    // mount fs
    nfs::nfs test_fs;

    co_await test_fs.mount(device_id);
    test_fs.print();

    // get root and check metadata
    shared_ptr<nfs::nfs_inode> root_inode = *co_await test_fs.get_root();
    kernel_assert(root_inode, "test_nfs_coro: root_inode is null");
    debugf("test_nfs_coro: get root done: %d", root_inode->inode_number);
    {
        reference_guard<nfs::nfs_inode> root_inode_ref =
            *co_await root_inode->get_ref();
        debugf("test_nfs_coro: root_inode: %d", root_inode_ref->inode_number);
        auto metadata1 = *co_await root_inode_ref->get_metadata();
        kernel_assert(metadata1->type == inode::ITYPE_DIR,
                      "metadata.type != T_DIR");
        root_inode_ref->print();
    }

    shared_ptr<dentry> root_dentry = root_inode->get_dentry().lock();
    kernel_assert(root_dentry, "test_nfs_coro: root_dentry is null");

    // create simple file
    shared_ptr<dentry> file_dentry =
        *co_await kernel_dentry_cache.create(root_dentry, "test_file", nullptr);
    kernel_assert(file_dentry, "test_nfs_coro: file_dentry is null");
    debugf("test_nfs_coro: create file_dentry done");

    {
        debugf("test_nfs_coro: create file...");
        reference_guard<nfs::nfs_inode> root_inode_ref =
            *co_await root_inode->get_ref();
        root_inode_ref->print();
        co_await root_inode_ref->create(file_dentry);
        debugf("test_nfs_coro: create file done");
    }

    shared_ptr<inode> file_inode = file_dentry->get_inode();
    kernel_assert(file_inode, "test_nfs_coro: file_inode is null");

    {
        auto file_inode_ref = *co_await file_inode->get_ref();
        // kernel_dentry_cache.put(file_dentry);

        int64 write_size =
            *co_await file_inode_ref->write("hello world", 0, 12);
        if (write_size != 12) {
            warnf("nfs: write failed:%d", write_size);
            co_return task_fail;
        }
        debugf("nfs: write file inode %d done", file_inode_ref->inode_number);
    }

    // find file using path
    shared_ptr<dentry> file_dentry2 =
        *co_await kernel_dentry_cache.get_at(nullptr, "/test_file");
    file_inode = file_dentry2->get_inode();
    {
        auto file_inode_ref = *co_await file_inode->get_ref();

        auto metadata2 = *co_await file_inode_ref->get_metadata();
        kernel_assert(metadata2->type == inode::ITYPE_FILE,
                      "metadata.type != T_FILE");
    }

    {
        // test file rw
        simple_file _file(file_inode);
        co_await _file.open();
        file_inode.reset();

        int64 file_size = *co_await _file.llseek(0, file::whence_t::END);

        // debugf("test_nfs_coro: file_size: %d", file_size);
        kernel_assert(file_size == 12, "test_nfs_coro: file_size != 12");
        char buf[13];
        buf[12] = 0;
        co_await _file.llseek(0, file::whence_t::SET);
        int64 read_size = *co_await _file.read(buf, file_size);

        debugf("test_nfs_coro: read_size: %d", read_size);
        debugf("test_nfs_coro: buf: %s", buf);
        co_await _file.close();
        debugf("file closed");
    }

    // create dir
    shared_ptr<dentry> dir_dentry =
        *co_await kernel_dentry_cache.create(root_dentry, "test_dir", nullptr);
    debugf("dir_dentry created");
    kernel_assert(dir_dentry, "test_nfs_coro: dir_dentry is null");

    {
        auto root_inode_ref = *co_await root_inode->get_ref();

        co_await root_inode_ref->mkdir(dir_dentry);
        debugf("test_nfs_coro: create dir done");
    }

    shared_ptr<inode> dir_inode = dir_dentry->get_inode();
    kernel_assert(dir_inode, "test_nfs_coro: dir_inode is null");

    co_await dir_inode->get();

    // create 2 files
    shared_ptr<dentry> file_dentry3 =
        *co_await kernel_dentry_cache.create(dir_dentry, "test_file3", nullptr);
    shared_ptr<dentry> file_dentry4 =
        *co_await kernel_dentry_cache.create(dir_dentry, "test_file4", nullptr);

    co_await dir_inode->create(file_dentry3);
    co_await dir_inode->create(file_dentry4);

    debugf("test_nfs_coro: create 2 files done");

    // read dir
    {
        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                   dentry->get_inode()->inode_number);
            count++;
        }

        kernel_assert(count == 2, "test_nfs_coro: count != 2");
    }

    // link file
    shared_ptr<dentry> file_dentry_link = *co_await kernel_dentry_cache.create(
        dir_dentry, "test_file3_link", nullptr);
    co_await dir_inode->link(file_dentry3, file_dentry_link);

    debugf("test_nfs_coro: link file done");

    // read dir
    {
        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                   dentry->get_inode()->inode_number);
            count++;
        }

        kernel_assert(count == 3, "test_nfs_coro: count != 3");
    }

    // test write linked file
    {
        simple_file _file2(file_dentry_link->get_inode());

        co_await _file2.open();
        co_await _file2.llseek(0, file::whence_t::SET);
        int64 write_size2 = *co_await _file2.write("hello world", 12);
        co_await _file2.close();

        kernel_assert(write_size2 == 12, "test_nfs_coro: write_size2 != 12");
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

        kernel_assert(read_size2 == 12, "test_nfs_coro: read_size2 != 12");

        debugf("test_nfs_coro: buf2: %s", buf2);
    }

    // test unlink file (delete file)
    {
        co_await dir_inode->unlink(file_dentry4);


        auto dir_iterator = dir_inode->read_dir();

        int count = 0;
        while (auto dentry = *co_await dir_iterator) {
            debugf("test_nfs_coro: read dir: %s %d", dentry->name.data(),
                   dentry->get_inode()->inode_number);
            count++;
        }

        kernel_assert(count == 2, "test_nfs_coro: count != 2");
    }

    dir_inode->put();
    dir_inode.reset();
    root_inode.reset();


    co_await test_fs.unmount();

    debugf("test_nfs_coro: done");

    infof("inode inner cache: %d/%d", nfs::nfs_inode::cache_hit,
          nfs::nfs_inode::cache_miss);

    // co_await test_nfs2_coro(device_id);

    debugf("test_nfs: ok");

    co_return task_ok;
}

void test_nfs2(void*) {
    // kernel_allocator.set_debug(true);
    // kernel_task_queue.push(test_nfs_coro(virtio_disk_id));

    // kernel_task_scheduler[0].schedule(std::move(test_coro(&a)));
    kernel_task_scheduler[0].schedule(std::move(test_nfs_coro(virtio_disk_id)));

    infof("test_nfs: push ok");
}