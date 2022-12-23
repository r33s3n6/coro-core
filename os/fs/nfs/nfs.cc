
#include <arch/riscv.h>
#include <ccore/types.h>

#include "inode.h"
#include "nfs.h"

#include <device/block/bdev.h>
#include <device/block/buf.h>

#include <mm/utils.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <utils/bitmap.h>

#include <fs/inode_cache.h>

namespace nfs {




void nfs::print() {
    infof ("nfs::print\nsuperblock:\ndirty: %d\ntotal_blocks: %l\ntotal_generic_blocks: %l\nused_generic_blocks: %l\nninodes: %l\ngeneric_block_start: %l\nnext_free_bitmap: %l\ninode_table:\n\tsize: %l\n\taddrs[0]:%d\n\tnext_addr_block:%d"
    , sb_dirty, sb.total_blocks, sb.total_generic_blocks, sb.used_generic_blocks, sb.ninodes, sb.generic_block_start, sb.next_free_bitmap, sb.inode_table.size, sb.inode_table.addrs[0], sb.inode_table.next_addr_block);
}


task<void> nfs::mount(device_id_t _device_id) {
    auto bdev = device::get<block_device>(_device_id);
    dev = bdev;

    auto buf_ptr = *co_await kernel_block_buffer.get(bdev, SUPER_BLOCK_INDEX);
    auto buf_ref = *co_await buf_ptr->get_ref();

    superblock *temp_sb = (superblock*)buf_ref->data;
    if(temp_sb->magic != NFS_MAGIC) {
        warnf("nfs: magic number mismatch: %p", (void*)(uint64)temp_sb->magic);

        co_return task_fail;
    }

    sb = *temp_sb;

    unmounted = false;
    wait_count = -1;
    sb_dirty = false;

        // Debug:
    debugf("kernel_inode_cache size:%d", kernel_inode_cache.size());
    debugf("kernel_dentry_cache size:%d", kernel_dentry_cache.size);

    
    root_inode = *co_await get_inode(ROOT_INODE_NUMBER);
    co_await kernel_dentry_cache.create(nullptr, "", root_inode);

    inode_table = make_shared<nfs_inode>(this, INODE_TABLE_NUMBER);

    

    co_return task_ok;
}

// flush all buffers and write superblock
task<void> nfs::unmount() {
    auto bdev = (block_device*)(dev);

    lock.lock();
    if (unmounted) {
        lock.unlock();
        co_return task_ok;
    }
    unmounted = true;
    lock.unlock();

    // we can assume nobody would (1) write sb, (2) create inode, after we set unmounted to true
    // co_await put_inode(root_inode);
    {
        auto root_inode_ref = *co_await root_inode->get_ref();
        root_inode->print();
    }

    root_inode = nullptr;

    co_await kernel_dentry_cache.destroy();

    
    int32 failed_count;
    failed_count = *co_await kernel_inode_cache.destroy(this, -1);

    // do {
    //     failed_count = *co_await kernel_inode_cache.destroy(this, -1);
    // } while (failed_count > 0);

    //this lock is meant for race from put_inode
    if (failed_count) {
        lock.lock();
        no_ref_count = 0;
        wait_count = failed_count;
        warnf("nfs: waiting for %d inodes to be released", failed_count);

        while(no_ref_count != failed_count) {
           co_await no_ref_wait_queue.done(lock);
        }
        lock.unlock();
    }
    
    

    co_await inode_table->flush();
    inode_table = nullptr;

    if (sb_dirty) {
        auto buf_ptr = *co_await kernel_block_buffer.get(bdev, SUPER_BLOCK_INDEX);
        auto buf_ref = *co_await buf_ptr->get_ref();

        *(superblock*)(buf_ref->data) = sb;
        buf_ref->mark_dirty();

        sb_dirty = false;
    }

    print();

    

    debugf("nfs: destroy block buffer");
    co_await kernel_block_buffer.destroy(dev->device_id);
    debugf("nfs: destroy block buffer done");

    
    dev = nullptr;
    co_return task_ok;
}

task<shared_ptr<inode>> nfs::get_root() {
    co_return root_inode;
}

task<void> nfs::make_fs(device_id_t device_id, uint32 nblocks) {
    auto bdev = device::get<block_device>(device_id);
    // this->dev = bdev;

    nfs temp_fs;
    temp_fs.dev = bdev;
    temp_fs.unmounted = false;

    if (nblocks < 1024) {
        co_return task_fail;
    }
   
    temp_fs.sb.magic = NFS_MAGIC;
    temp_fs.sb.total_blocks = nblocks;
    temp_fs.sb.total_generic_blocks =  (nblocks - 2) * BITMAP_BLOCK_SIZE / (BITMAP_BLOCK_SIZE + 1);
    temp_fs.sb.used_generic_blocks = 0;
    temp_fs.sb.ninodes = 2; // root and inode table
    temp_fs.sb.generic_block_start = 2 + nblocks - temp_fs.sb.total_generic_blocks;
    temp_fs.sb.next_free_bitmap = BITMAP_BLOCK_INDEX;

    temp_fs.sb_dirty = true;

    // set all bitmap blocks to 0
    for (uint32 i = BITMAP_BLOCK_INDEX; i < temp_fs.sb.generic_block_start; i++) {
        auto buf_ptr = *co_await kernel_block_buffer.get(bdev, i);
        auto buf_ref = *co_await buf_ptr->get_ref();
        memset(buf_ref->data, 0, BLOCK_SIZE);
        buf_ref->mark_dirty();
    }

    {
        auto buf_ptr = *co_await kernel_block_buffer.get(bdev, BITMAP_BLOCK_INDEX);
        auto buf_ref = *co_await buf_ptr->get_ref();
        auto buf = (uint64*)buf_ref->data;
        for (uint32 i = 0; i < temp_fs.sb.generic_block_start; i++) {
            bitmap_set(buf, i);
        }
        buf_ref->mark_dirty();
    }
    

    temp_fs.inode_table = make_shared<nfs_inode>(&temp_fs, INODE_TABLE_NUMBER);
    temp_fs.inode_table->init_data();
    temp_fs.inode_table->metadata.type = inode::ITYPE_FILE;
    temp_fs.inode_table->metadata.nlinks = 1;
    temp_fs.inode_table->mark_dirty();
    //temp_fs.inode_table->mark_valid();
    debugf("nfs::make_fs: inode_table: %p\n", temp_fs.inode_table.get());
    
    dinode temp_dinode;
    temp_dinode.type = inode::ITYPE_NEXT_FREE_INODE;
    temp_dinode.nlinks = 1;
    temp_dinode.size = ROOT_INODE_NUMBER+1;
    co_await temp_fs.inode_table->write(&temp_dinode, 0, sizeof(dinode));


    temp_fs.root_inode = *co_await temp_fs.get_inode(ROOT_INODE_NUMBER);
    temp_fs.root_inode->init_data();
    temp_fs.root_inode->metadata.type = inode::ITYPE_DIR;
    temp_fs.root_inode->metadata.nlinks = 1;
    temp_fs.root_inode->mark_dirty();
    //temp_fs.root_inode->mark_valid();
    debugf("nfs::make_fs: root_inode: %p\n", temp_fs.root_inode.get());


    debugf("nfs::make_fs: waiting for unmount\n");
    co_await temp_fs.unmount();

    co_return task_ok;
    
}

task<uint32> nfs::alloc_block() {
    auto bdev = (block_device*)(dev);
    
    lock.lock();
    if (sb.used_generic_blocks >= sb.total_generic_blocks) {
        lock.unlock();
        co_return task_fail;
    }
    if(sb.next_free_bitmap >= sb.generic_block_start) {
        sb.next_free_bitmap = BITMAP_BLOCK_INDEX;
    }


    uint32 bitmap_index = sb.next_free_bitmap;
    lock.unlock();

    uint32 __block_index;
    block_buffer_t::buffer_ptr_t buf_ptr;
    block_buffer_t::buffer_ref_t buf_ref;

    void* temp_save;
    void* temp_save_bitmap;

    for (; bitmap_index < sb.generic_block_start; bitmap_index++){
        buf_ptr = *co_await kernel_block_buffer.get(bdev, bitmap_index);
        buf_ref = *co_await buf_ptr->get_ref();
        const uint64* const_bitmap = (const uint64*)buf_ref->data;

        __block_index = bitmap_find_first_zero(const_bitmap, BLOCK_SIZE / sizeof(uint64));

        if(__block_index != (uint32)-1){
            break;
        }
    }

    uint32 block_index = (bitmap_index - BITMAP_BLOCK_INDEX) * BITMAP_BLOCK_SIZE + __block_index;

    if (block_index >= sb.total_blocks) {
        warnf("nfs: alloc_block: block_index >= sb.total_blocks: %d >= (%d/%d), %p", block_index,sb.used_generic_blocks, sb.total_blocks, temp_save);
        co_return task_fail;
    }

    uint64* bitmap = (uint64*)buf_ref->data;

    bitmap_set(bitmap, __block_index);
    buf_ref->mark_dirty();
    buf_ref->put();
    buf_ptr.reset(nullptr);
    

    if (block_index < sb.generic_block_start) {
        warnf("nfs: alloc_block: block_index < sb.generic_block_start: %d < %d, %p, %p, %p", block_index, sb.generic_block_start, temp_save, (void*)bitmap, temp_save_bitmap);
        panic("nfs: alloc_block: block_index < sb.generic_block_start");
    }

    lock.lock();

    // if(unmounted) {
    //     lock.unlock();
    //     co_return task_fail;
    // }


    
    sb.used_generic_blocks++;
    sb.next_free_bitmap = bitmap_index;
    sb_dirty = true;

    lock.unlock();
    
    // infof("nfs: alloc_block : %d", block_index);
    co_return block_index;

}

task<void> nfs::free_block(uint32 block_index) {

    auto bdev = (block_device*)(dev);

    // debugf("nfs: free_block: %d", block_index);

    if (block_index >= sb.total_blocks) {
        warnf("nfs: free_block: block_index >= sb.total_blocks: %d >= %d", block_index, sb.total_blocks);
        co_return task_fail;
    }

    uint32 bitmap_index = BITMAP_BLOCK_INDEX + block_index / BITMAP_BLOCK_SIZE;
    uint32 __block_index = block_index % BITMAP_BLOCK_SIZE;

    auto buf_ptr = *co_await kernel_block_buffer.get(bdev, bitmap_index);
    auto buf_ref = *co_await buf_ptr->get_ref();
    uint64* bitmap = (uint64*)buf_ref->data;

    lock.lock();

    // if(unmounted) {
    //     lock.unlock();
    //     
    //     co_return task_fail;
    // }

    bitmap_clear(bitmap, __block_index);
    buf_ref.put();
    buf_ptr.reset(nullptr);
    
    sb.used_generic_blocks--;
    // sb.next_free_bitmap = bitmap_index;

    sb_dirty = true;
    lock.unlock();

    // debugf("nfs: free_block: done");

    co_return task_ok;
}

task<nfs::inode_ptr_t> nfs::alloc_inode() {
    // traverse the inode table, find a free inode
    dinode temp_dinode;
    dinode new_dinode;

    uint32 next_free_inode;
    {
        auto inode_table_ref = *co_await inode_table->get_ref();

        co_await inode_table_ref->read(&temp_dinode, 0, sizeof(dinode));

        if (temp_dinode.type != inode::ITYPE_NEXT_FREE_INODE) {
            warnf("nfs: alloc_inode: temp_dinode.type != inode::ITYPE_NEXT_FREE_INODE: %d != %d", temp_dinode.type, inode::ITYPE_NEXT_FREE_INODE);
            co_return task_fail;
        }

        next_free_inode = temp_dinode.size;

        co_await inode_table_ref->read(&new_dinode, next_free_inode*sizeof(dinode), sizeof(dinode));

        if (new_dinode.type == inode::ITYPE_NEXT_FREE_INODE) {
            temp_dinode.size = new_dinode.size;
        } else {
            temp_dinode.size = next_free_inode + 1;
        }
        co_await inode_table_ref->write(&temp_dinode, 0, sizeof(dinode));

    }

    auto new_inode = *co_await get_inode(next_free_inode);
    new_inode->init_data();

    lock.lock();
    sb.ninodes++;
    sb_dirty = true;
    lock.unlock();

    co_return new_inode;
    
}

task<void> nfs::free_inode(uint32 inode_index) { 

    debugf("nfs: free_inode: %d", inode_index);

    // traverse the inode table, find a free inode
    dinode temp_dinode;
    dinode new_dinode;
    {
        auto inode_table_ref = *co_await inode_table->get_ref();

        co_await inode_table_ref->read(&temp_dinode, 0, sizeof(dinode));

        if (temp_dinode.type != inode::ITYPE_NEXT_FREE_INODE) {
            warnf("nfs: alloc_inode: temp_dinode.type != inode::ITYPE_NEXT_FREE_INODE: %d != %d", temp_dinode.type, inode::ITYPE_NEXT_FREE_INODE);
            co_return task_fail;
        }

        new_dinode.size = temp_dinode.size;
        new_dinode.type = inode::ITYPE_NEXT_FREE_INODE;

        temp_dinode.size = inode_index;
        // write back

        co_await inode_table_ref->write(&temp_dinode, 0, sizeof(dinode));
        co_await inode_table_ref->write(&new_dinode, inode_index*sizeof(dinode), sizeof(dinode));

    }

    lock.lock();
    sb.ninodes--;
    sb_dirty = true;
    lock.unlock();

    co_return task_ok;
}

task<nfs::inode_ptr_t> nfs::get_inode(uint32 inode_number) {
    auto guard = make_lock_guard(lock);
    if (unmounted) {
        co_return inode_ptr_t{};
    }

    auto inode_ptr = *co_await kernel_inode_cache.get_derived<inode_t>(this, inode_number);

    co_return inode_ptr;
}

// called when inode destruct
task<void> nfs::put_inode(inode_ptr_t inode_ptr){ 

    co_await inode_ptr->flush();

    __put_inode(inode_ptr->inode_number);
    co_return task_ok;

}


// called when inode destruct
void nfs::__put_inode(uint32 inode_number){ 
    (void)(inode_number);
    

    auto guard = make_lock_guard(lock);

    if (unmounted && wait_count != -1) {
        no_ref_count++;

        if (no_ref_count == wait_count) {
            no_ref_wait_queue.wake_up();
        }
    }

}



} // namespace nfs

