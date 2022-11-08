
#include <arch/riscv.h>
#include <ccore/types.h>

#include "nfs.h"
#include "inode.h"

#include <device/block/bdev.h>
#include <device/block/buf.h>

#include <mm/utils.h>
#include <utils/log.h>
#include <utils/assert.h>

namespace nfs {




void nfs::print() {
    infof ("nfs::print\nsuperblock:\ndirty: %d\ntotal_blocks: %l\ntotal_generic_blocks: %l\nused_generic_blocks: %l\nninodes: %l\ngeneric_block_start: %l\nnext_free_bitmap: %l\ninode_table:\n\tsize: %l\n\taddrs[0]:%d\n\tnext_addr_block:%d"
    , sb_dirty, sb.total_blocks, sb.total_generic_blocks, sb.used_generic_blocks, sb.ninodes, sb.generic_block_start, sb.next_free_bitmap, sb.inode_table.size, sb.inode_table.addrs[0], sb.inode_table.next_addr_block);
}

// size: number of elements in uint64[]
uint32 bitmap_find_first_zero(const uint64 *bitmap, uint32 size) {
    uint32 i;
    for (i = 0; i < size; i++) {
        if (bitmap[i] != ~(0uL)) 
            break;
    }
    if (i == size)
        return -1;

    uint32 j;
    for (j = 0; j < 64; j++) {
        if (!(bitmap[i] & (1uL << j)))
            break;
    }
    return i * 64 + j;
}

void bitmap_set(uint64 *bitmap, uint32 index) {
    bitmap[index / 64] |= (1uL << (index % 64));
}

void bitmap_clear(uint64 *bitmap, uint32 index) {
    bitmap[index / 64] &= ~(1uL << (index % 64));
}

uint32 bitmap_get(const uint64 *bitmap, uint32 index) {
    return bitmap[index / 64] & (1uL << (index % 64));
}

task<void> nfs::mount(device_id_t _device_id) {
    device_id = _device_id;

    auto buf_ref = *co_await kernel_block_buffer.get_node(device_id, SUPER_BLOCK_INDEX);

    auto buf = *co_await buf_ref.get_for_read();

    superblock *temp_sb = (superblock*)buf;
    if(temp_sb->magic != NFS_MAGIC) {
        warnf("nfs: magic number mismatch: %p", (void*)(uint64)temp_sb->magic);

        co_return task_fail;
    }

    sb = *temp_sb;

    unmounted = false;
    sb_dirty = false;

    
    root_inode = get_inode(ROOT_INODE_NUMBER);
    inode_table = new nfs_inode(this, INODE_TABLE_NUMBER);
    co_return task_ok;
}

// flush all buffers and write superblock
task<void> nfs::unmount() {
    lock.lock();
    unmounted = true;
    lock.unlock();

    // we can assume nobody would (1) write sb, (2) create inode, after we set unmounted to true

    co_await put_inode(root_inode);
    root_inode = nullptr;

    // this lock is meant for race from put_inode
    lock.lock();

    no_ref_count = 0;
    for (auto inode : inode_list) {
        uint64 ref_count = inode->get_ref();
        if (ref_count == 0) {
            no_ref_count++;
        } else {
            debugf("nfs: inode %d has %d references", inode->inode_number, ref_count);
        }
    }

    while(no_ref_count != (uint32)inode_list.size()) {
        debugf("nfs: waiting for %d/%d inodes to be released", inode_list.size() - no_ref_count, inode_list.size());
        co_await no_ref_wait_queue.done(lock); // unlock and let put_inode to wake us up
    }

    lock.unlock();

    // destroy all inodes pointing to this device
    for (auto inode : inode_list) {
        co_await inode->flush();
        delete inode;
    }
    
    inode_list.clear();
    

    co_await inode_table->flush();
    delete inode_table;

    if (sb_dirty) {
        auto buf_ref = *co_await kernel_block_buffer.get_node(device_id, SUPER_BLOCK_INDEX);
        auto buf = *co_await buf_ref.get_for_write();
        *(superblock*)buf = sb;
        sb_dirty = false;
    }

    

    debugf("nfs: destroy block buffer");
    co_await kernel_block_buffer.destroy(device_id);
    debugf("nfs: destroy block buffer done");

    
    device_id = {};
    co_return task_ok;
}

task<inode*> nfs::get_root() {
    co_return (inode*)root_inode;
}

task<void> nfs::make_fs(device_id_t device_id, uint32 nblocks) {
    nfs temp_fs;
    temp_fs.device_id = device_id;
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
        auto buf_ref = *co_await kernel_block_buffer.get_node(device_id, i);
        auto buf = *co_await buf_ref.get_for_write();
        memset(buf, 0, BLOCK_SIZE);
    }

    {
        auto buf_ref = *co_await kernel_block_buffer.get_node(device_id, BITMAP_BLOCK_INDEX);
        auto buf = *co_await buf_ref.get_for_write();
        for (uint32 i = 0; i < temp_fs.sb.generic_block_start; i++) {
            bitmap_set((uint64*)buf, i);
        }
    }
    

    temp_fs.inode_table = new nfs_inode(&temp_fs, INODE_TABLE_NUMBER);
    temp_fs.inode_table->init();
    temp_fs.inode_table->metadata.type = T_FILE;


    temp_fs.root_inode = temp_fs.get_inode(ROOT_INODE_NUMBER);
    temp_fs.root_inode->init();
    temp_fs.root_inode->metadata.type = T_DIR;

    // create one example file
    // TODO inumber alloc
    auto file_inode = temp_fs.get_inode(2);
    temp_fs.sb.ninodes++;

    file_inode->init();
    file_inode->metadata.type = T_FILE;
    debugf("nfs: write file inode %d", file_inode->inode_number);
    int64 write_size = *co_await file_inode->write("hello world", 0, 12);
    if (write_size != 12) {
        warnf("nfs: write failed:%d", write_size);
        co_return task_fail;
    }
    debugf("nfs: write file inode %d done", file_inode->inode_number);

    dirent temp_file_dirent;
    temp_file_dirent.inode_number = 2;
    strncpy(temp_file_dirent.name, "hello", 6);
    
    co_await temp_fs.root_inode->write(&temp_file_dirent, 0, sizeof(dirent));

    co_await temp_fs.put_inode(file_inode);

    
    
    debugf("nfs::make_fs: waiting for unmount\n");
    co_await temp_fs.unmount();
    
}

task<uint32> nfs::alloc_block() {
    
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
    block_buffer_node_ref buf_ref;

    void* temp_save;
    void* temp_save_bitmap;

    for (; bitmap_index < sb.generic_block_start; bitmap_index++){
        buf_ref = *co_await kernel_block_buffer.get_node(device_id, bitmap_index);
        const uint64* const_bitmap = (const uint64*)*co_await buf_ref.get_for_read();

        __block_index = bitmap_find_first_zero(const_bitmap, BLOCK_SIZE / sizeof(uint64));

        if(__block_index != (uint32)-1){
            break;
        }
    }

    uint32 block_index = (bitmap_index - BITMAP_BLOCK_INDEX) * BITMAP_BLOCK_SIZE + __block_index;

    if (block_index >= sb.total_blocks) {
        warnf("nfs: alloc_block: block_index >= sb.total_blocks: %d >= %d, %p", block_index, sb.total_blocks, temp_save);
        co_return task_fail;
    }

    uint64* bitmap = (uint64*)*co_await buf_ref.get_for_write();

    bitmap_set(bitmap, __block_index);

    // debugf("set bitmap %d %d", bitmap_index, __block_index);
    // debugf("first 2 8-bytes: %p %p", (void*)bitmap[0], (void*)bitmap[1]);
    
    buf_ref.put();

    

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
    __sync_synchronize();
    co_return block_index;

}

task<void> nfs::free_block(uint32 block_index) {

    if (block_index >= sb.total_blocks) {
        co_return task_fail;
    }

    uint32 bitmap_index = BITMAP_BLOCK_INDEX + block_index / BITMAP_BLOCK_SIZE;
    uint32 __block_index = block_index % BITMAP_BLOCK_SIZE;

    auto buf_ref = *co_await kernel_block_buffer.get_node(device_id, bitmap_index);
    uint64* bitmap = (uint64*)*co_await buf_ref.get_for_write();

    lock.lock();

    if(unmounted) {
        lock.unlock();
        co_return task_fail;
    }

    bitmap_clear(bitmap, __block_index);
    buf_ref.put();
    
    sb.used_generic_blocks--;
    // sb.next_free_bitmap = bitmap_index;

    sb_dirty = true;
    lock.unlock();

    co_return task_ok;
}

nfs_inode* nfs::get_inode(uint32 inode_number) {
    auto guard = make_lock_guard(lock);
    if (unmounted) {
        return nullptr;
    }
    for (auto &inode : inode_list) {
        if (inode->inode_number == inode_number) {
            inode->inc_ref();
            return inode;
        }
    }
    auto new_inode = new nfs_inode(this, inode_number);
    new_inode->inc_ref();

    inode_list.push_front(new_inode);

    return new_inode;
}
task<void> nfs::put_inode(nfs_inode *inode){
    auto guard = make_lock_guard(lock);
    inode->dec_ref();
    if (unmounted) {
        if (inode->get_ref() == 0) {
            no_ref_count++;
            // co_await inode->flush()
        }
        if (no_ref_count == (uint32)inode_list.size()) {
            no_ref_wait_queue.wake_up();
        }
    }
    co_return task_ok;
}



}