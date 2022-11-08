#include <device/block/bdev.h>
#include <device/block/buf.h>

#include <mm/utils.h>
#include <utils/assert.h>

#include <utils/log.h>

#include "inode.h"

// TODO: rw_lock
namespace nfs {

uint32 nfs_inode::cache_hit = 0;
uint32 nfs_inode::cache_miss = 0;

void nfs_inode::print(){
    infof("nfs_inode::print\ninode_number: %d\nperm: %d\nsize: %d\ntype: %d\naddrs[0]: %d\nnext_addr_block: %d", inode_number, metadata.perm, metadata.size, metadata.type, addrs[0], next_addr_block);
}

template <bool _write>
task<void> nfs_inode::direct_data_block_rw(
    uint32 direct_offset, uint32 data_block_offset,
    std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size) { 

    kernel_assert(direct_offset < NUM_DIRECT_DATA, "direct_offset out of range");
    kernel_assert(data_block_offset + size <= block_device::BLOCK_SIZE, "size out of range");

    uint32 data_block_index = addrs[direct_offset];

    // debugf("inode %d: get data block index: %d from direct offset: %d",inode_number, data_block_index, direct_offset);

    bool clear = false;

    if(data_block_index == (uint32)-1){
        if constexpr (_write){
            
            // allocate a new block
            data_block_index = *co_await _fs->alloc_block();
            addrs[direct_offset] = data_block_index;
            metadata_dirty = true;
            clear = true;
        } else {
            // all zero block
            memset(buf, 0, size);
            co_return task_ok;
        }
    }

    auto block_buf_ref = *co_await kernel_block_buffer.get_node(device_id, data_block_index);

    
    

    if constexpr (_write) {
        auto block_buf = *co_await block_buf_ref.get_for_write();
        memcpy(block_buf + data_block_offset, buf, size);
        if(clear) {
            memset(block_buf, 0, data_block_offset);
            memset(block_buf + data_block_offset + size, 0, block_device::BLOCK_SIZE - data_block_offset - size);
        }
    } else {
        auto block_buf = *co_await block_buf_ref.get_for_read();
        memcpy(buf, block_buf + data_block_offset, size);
    }

    co_return task_ok;
}

template <bool _write>
task<void> nfs_inode::data_block_rw(
    uint32 addr_block_index, uint32 addr_block_offset, uint32 data_block_offset,
    std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size) { 
    
    kernel_assert(addr_block_offset < NUM_ADDRS, "addr_block_offset out of range");
    kernel_assert(data_block_offset + size <= block_device::BLOCK_SIZE, "size out of range");

    uint32 data_block_index;
    bool clear = false;

    {
        // read addr block
        auto addr_block_buf_ref = *co_await kernel_block_buffer.get_node(device_id, addr_block_index);
        auto addr_block_buf = *co_await addr_block_buf_ref.get_for_read();

        const addr_block *_addr_block = (const addr_block*)addr_block_buf;

        data_block_index = _addr_block->addrs[addr_block_offset];

        if(data_block_index == (uint32)-1){
            if constexpr (_write){
                // allocate a new block
                data_block_index = *co_await _fs->alloc_block();
                addr_block* _addr_block = (addr_block*)*co_await addr_block_buf_ref.get_for_write();
                _addr_block->addrs[addr_block_offset] = data_block_index;
                clear = true;
            } else {
                // all zero block
                memset(buf, 0, size);
                co_return task_ok;
            }
        }
    }
    
    // if(data_block_offset == 0){
    //     if (data_block_index % 30 == 0) {
    //         debugf("inode %d: get data block index: %d from addr block index: %d, offset: %d",inode_number, data_block_index, addr_block_index, addr_block_offset);
    //     }
// 
    // }


    auto block_buf_ref = *co_await kernel_block_buffer.get_node(device_id, data_block_index);

    
    if constexpr (_write) {
        auto block_buf = *co_await block_buf_ref.get_for_write();
        memcpy(block_buf + data_block_offset, buf, size);
        if(clear) {
            memset(block_buf, 0, data_block_offset);
            memset(block_buf + data_block_offset + size, 0, block_device::BLOCK_SIZE - data_block_offset - size);
        }
    } else {
        auto block_buf = *co_await block_buf_ref.get_for_read();
        memcpy(buf, block_buf + data_block_offset, size);
    }



    co_return task_ok;

}

template <bool _write>
task<int64> nfs_inode::data_rw(std::conditional_t<_write, const uint8 *, uint8*> buf, uint64 offset, uint64 size) {

    int64 rw_size = 0;
    uint32 block_rw_size = 0;

    uint32 addr_block_index = 0;
    uint32 addr_block_offset = 0;
    // uint32 data_block_index = 0;
    uint32 data_block_offset = 0;


    if(!metadata_valid) {
        co_await load();
    }


    while (offset < DIRECT_DATA_SIZE && size > 0) {
        addr_block_offset = offset / BLOCK_SIZE;
        data_block_offset = offset % BLOCK_SIZE;
        block_rw_size = std::min(size, (uint64)BLOCK_SIZE - data_block_offset);


        co_await direct_data_block_rw<_write>(addr_block_offset, data_block_offset, buf, block_rw_size);
        
        offset += block_rw_size;
        buf += block_rw_size;
        size -= block_rw_size;
        rw_size += block_rw_size;
    }

    

    if(size == 0) {
        co_return rw_size;
    }

    if (next_addr_block == (uint32)-1) {
        if constexpr (_write) {
            // allocate a new block
            next_addr_block = *co_await _fs->alloc_block();
            metadata_dirty = true;

            // init next_addr_block with -1
            auto next_addr_block_buf_ref = *co_await kernel_block_buffer.get_node(device_id, next_addr_block);
            auto next_addr_block_buf = *co_await next_addr_block_buf_ref.get_for_write();
            memset(next_addr_block_buf, 0xff, block_device::BLOCK_SIZE);
        } else {
            // all zero block
            memset(buf, 0, size);
            rw_size += size;
            co_return rw_size;
        }
    }
    // now next_addr_block is valid
    uint32 current_addr_block = next_addr_block;

    
    offset -= DIRECT_DATA_SIZE;


    uint64 complete_indirect_offset = offset + size;

    if (cache_raw_indirect_offset == offset) {
        nfs_inode::cache_hit++;
        current_addr_block = cache_addr_block;
        offset = cache_offset;
    } else {
        nfs_inode::cache_miss++;
    }

    // now offset is relative to the start of current_addr_block

    while (size > 0) {


        co_await get_block_index(
            current_addr_block, offset, 
            &addr_block_index, &addr_block_offset, 
            nullptr, &data_block_offset);
        

        

        current_addr_block = addr_block_index;

        offset %= ADDR_BLOCK_DATA_SIZE;

        block_rw_size = std::min(size, (uint64)BLOCK_SIZE - data_block_offset);


        co_await data_block_rw<_write>(
            addr_block_index, addr_block_offset, data_block_offset, 
            buf, block_rw_size);
        
        
        buf += block_rw_size;
        size -= block_rw_size;
        rw_size += block_rw_size;
        offset += block_rw_size;


    }

    cache_raw_indirect_offset = complete_indirect_offset;
    cache_offset = offset;
    cache_addr_block = current_addr_block;

    co_return rw_size;
}

task<int64> nfs_inode::read(void *dest, uint64 offset, uint64 size) {
    co_return *co_await data_rw<false>((uint8*)dest, offset, size);

}
task<int64> nfs_inode::write(const void *src, uint64 offset, uint64 size) {
    int64 write_size = *co_await data_rw<true>((const uint8*)src, offset, size);
    if (offset + write_size > metadata.size) {
        metadata.size = offset + write_size;
        metadata_dirty = true;
    }
    co_return write_size;
}

task<const nfs_inode::metadata_t*> nfs_inode::get_metadata() {
    if (!metadata_valid) {
        // read inode information from disk
        co_await load();
    }

    co_return &metadata;
}

task<int32> nfs_inode::truncate(int64 size) {
    // resize inode
    (void)(size);
    co_return -EPERM;
}


task<int32> nfs_inode::create(dentry* new_dentry) {
    (void)(new_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::link(dentry* old_dentry, dentry* new_dentry) {
    (void)(old_dentry);
    (void)(new_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::symlink(dentry* old_dentry, dentry* new_dentry) {
    (void)(old_dentry);
    (void)(new_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::unlink(dentry* old_dentry ) {
    (void)(old_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::mkdir(dentry* new_dentry) {
    (void)(new_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::rmdir(dentry* old_dentry) {
   (void)(old_dentry);
    co_return -EPERM;
}

void nfs_inode::init() {
    kernel_assert(!metadata_valid && !metadata_dirty, "nfs_inode::init: metadata is valid or dirty");
    metadata_valid = true;
    metadata_dirty = true;
    metadata.size = 0;
    metadata.perm = 0;
    metadata.type = 0;
    metadata.nlinks = 0;

    memset(addrs, 0xff, sizeof(addrs));

    next_addr_block = (uint32)-1;
}

task<int32> nfs_inode::load() {
    if (metadata_valid) {
        co_return 0;
    }

    dinode dinode_buf;
    dinode* disk_inode;
    if (inode_number == INODE_TABLE_NUMBER) {
        // inode table, we read it from superblock
        disk_inode = &_fs->sb.inode_table;
    } else {
        co_await _fs->inode_table->read(&dinode_buf, inode_number * sizeof(dinode), sizeof(dinode));
        disk_inode = &dinode_buf;
    }

    // copy inode information
    metadata.size = disk_inode->size;
    metadata.type = disk_inode->type;
    metadata.nlinks = disk_inode->nlinks;
    metadata.perm = disk_inode->perm;

    // copy data block index
    for (uint32 i = 0; i < NUM_DIRECT_DATA; i++) {
        addrs[i] = disk_inode->addrs[i];
    }
    next_addr_block = disk_inode->next_addr_block;

    metadata_valid = true;
    metadata_dirty = false;

    co_return 0;
}
task<int32> nfs_inode::flush() {
    // assert metadata_valid
    if (!metadata_valid) {
        co_return -1;
    }

    if (!metadata_dirty) {
        co_return 0;
    }

    dinode dinode_buf;
    dinode* disk_inode;

    if (inode_number == INODE_TABLE_NUMBER) {
        // inode table, we write it to superblock
        disk_inode = &_fs->sb.inode_table;
    } else {
        disk_inode = &dinode_buf;
    }

    // copy inode information
    disk_inode->size = metadata.size;
    disk_inode->type = metadata.type;
    disk_inode->nlinks = metadata.nlinks;
    disk_inode->perm = metadata.perm;

    // copy data block index
    for (uint32 i = 0; i < NUM_DIRECT_DATA; i++) {
        disk_inode->addrs[i] = addrs[i];
    }
    disk_inode->next_addr_block = next_addr_block;

    if (inode_number == INODE_TABLE_NUMBER) {
        // inode table, we write it to superblock
        _fs->sb_dirty = true;
    } else {
        co_await _fs->inode_table->write(disk_inode, inode_number * sizeof(dinode), sizeof(dinode));
    }


    metadata_dirty = false;

    co_return 0;
}

// create intermediate addr blocks if necessary
task<void> nfs_inode::get_block_index(uint32 start_addr_block, uint64 offset,
         uint32* addr_block_index, uint32* addr_block_offset, uint32* data_block_index, uint32* data_block_offset) {

    uint32 _addr_block_index = start_addr_block;

    block_buffer_node_ref buf_ref;

    // find final index block
    while (true) {
        buf_ref = *co_await kernel_block_buffer.get_node(device_id, _addr_block_index);

        if (offset < ADDR_BLOCK_DATA_SIZE) {
            break;
        }

        const addr_block* buf = (const addr_block*)*co_await buf_ref.get_for_read();

        if (buf->next_addr_block == (uint32)-1) { 
            break;
        }

        offset -= ADDR_BLOCK_DATA_SIZE;
        _addr_block_index = buf->next_addr_block;
    }

    // create all intermediate addr blocks
    while (offset >= ADDR_BLOCK_DATA_SIZE) {
        // buf_ref hold the last addr block
        

        uint32 new_addr_block_index = *co_await _fs->alloc_block();
        debugf("nfs_inode::get_block_index: alloc new addr block %d", new_addr_block_index);
        auto new_buf_ref = *co_await kernel_block_buffer.get_node(device_id, new_addr_block_index);
        auto new_buf = *co_await new_buf_ref.get_for_write();
        memset(new_buf, 0xff, BLOCK_SIZE);
        const addr_block* debug_block = (const addr_block*)new_buf;
        debugf("nfs_inode::get_block_index: first 8 bytes: %p, next_addr_block %d", (void*)*(uint64*)debug_block, debug_block->next_addr_block);
        // kernel_assert(*(uint64*)new_buf == (uint64)-1, "nfs_inode::get_block_index: new_buf is nullptr");

        // update previous addr block
        addr_block* buf = (addr_block*)*co_await buf_ref.get_for_write();
        buf->next_addr_block = new_addr_block_index;

        _addr_block_index = new_addr_block_index;
        buf_ref = std::move(new_buf_ref);

        offset -= ADDR_BLOCK_DATA_SIZE;

        debugf("nfs_inode::get_block_index: create intermediate addr block: %d", _addr_block_index);

    }

    // now we hold the addr block ref


    // find real data block
    uint32 _addr_block_offset = offset / BLOCK_SIZE;

    // debug



    if (addr_block_index){
        *addr_block_index = _addr_block_index;
    }
    if (addr_block_offset) {
        *addr_block_offset = _addr_block_offset;
    }
    if (data_block_index) {
        // read index block
        const addr_block* buf = (const addr_block*)*co_await buf_ref.get_for_read();
        uint32 _data_block_index = buf->addrs[_addr_block_offset];

        *data_block_index = _data_block_index;
    }
    if(data_block_offset) {
        *data_block_offset = offset % BLOCK_SIZE;
    }

    co_return task_ok;
}

}


/*
 struct
{
    struct spinlock lock;
    struct inode inode[NINODE];
} itable;

static uint
bmap(struct inode *ip, uint bn);
static struct inode *
inode_or_parent_by_name(char *path, int nameiparent, char *name);

// Free a disk block.
static void free_disk_block(int dev, uint block_id) {
    struct buf *bitmap_block;
    int bit_offset; // in bitmap block
    int mask;

    bitmap_block = acquire_buf_and_read(dev, BITMAP_BLOCK_CONTAINING(block_id, sb));
    bit_offset = block_id % BITS_PER_BITMAP_BLOCK;
    mask = 1 << (bit_offset % 8);
    if ((bitmap_block->data[bit_offset / 8] & mask) == 0)
        panic("freeing free block");
    bitmap_block->data[bit_offset / 8] &= ~mask;
    write_buf_to_disk(bitmap_block);
    release_buf(bitmap_block);
}

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
//   skipelem("///a//bb", name) = "bb", setting name = "a"
//   skipelem("a", name) = "", setting name = "a"
//   skipelem("", name) = skipelem("////", name) = 0
//
static char *
skipelem(char *path, char *name) {
    char *s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= DIRSIZ)
        memmove(name, s, DIRSIZ);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

// Zero a block.
static void
set_block_to_zero(int dev, int bno) {
    struct buf *bp;
    bp = acquire_buf_and_read(dev, bno);
    memset(bp->data, 0, BSIZE);
    write_buf_to_disk(bp);
    release_buf(bp);
}

// Blocks.

// Allocate a zeroed disk block.
static uint
alloc_zeroed_block(uint dev) {
    int base_block_id;
    int local_block_id;
    int bit_mask;
    struct buf *bitmap_block;

    bitmap_block = nullptr;
    for (base_block_id = 0; base_block_id < sb.total_blocks; base_block_id += BITS_PER_BITMAP_BLOCK) {
        // the corresponding bitmap block
        bitmap_block = acquire_buf_and_read(dev, BITMAP_BLOCK_CONTAINING(base_block_id, sb));

        // iterate all bits in this bitmap block
        for (local_block_id = 0; local_block_id < BITS_PER_BITMAP_BLOCK && base_block_id + local_block_id < sb.total_blocks; local_block_id++) {
            bit_mask = 1 << (local_block_id % 8);
            if ((bitmap_block->data[local_block_id / 8] & bit_mask) == 0) {
                // the block free
                bitmap_block->data[local_block_id / 8] |= bit_mask; // Mark block in use.
                write_buf_to_disk(bitmap_block);
                release_buf(bitmap_block);
                set_block_to_zero(dev, base_block_id + local_block_id);
                return base_block_id + local_block_id;
            }
        }
        release_buf(bitmap_block);
    }
    panic("alloc_zeroed_block: out of blocks");
    return 0;
}


// Allocate an inode on device dev.
// Mark it as allocated by  giving it type `type`.
// Returns an allocated and referenced inode.
struct inode *
alloc_disk_inode(uint dev, short type) {
    int inum;
    struct buf *bp;
    struct dinode *disk_inode;

    for (inum = 1; inum < sb.ninodes; inum++) {
        bp = acquire_buf_and_read(dev, BLOCK_CONTAINING_INODE(inum, sb));
        disk_inode = (struct dinode *)bp->data + inum % INODES_PER_BLOCK;
        if (disk_inode->type == 0) {
            // a free inode
            memset(disk_inode, 0, sizeof(*disk_inode));
            disk_inode->type = type;
            write_buf_to_disk(bp);
            release_buf(bp);
            return iget(dev, inum);
        }
        release_buf(bp);
    }
    panic("ialloc: no inodes");
    return 0;
}

void inode_table_init() {
    init_spin_lock_with_name(&itable.lock, "itable");
    for (int i = 0; i < NINODE; i++) {
        init_mutex(&itable.inode[i].lock);
    }
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk.
void iupdate(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;

    bp = acquire_buf_and_read(ip->dev, BLOCK_CONTAINING_INODE(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % INODES_PER_BLOCK;
    dip->type = ip->type;
    dip->major = ip->major;
    dip->minor = ip->minor;
    dip->num_link = ip->num_link;
    dip->size = ip->size;
    memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
    write_buf_to_disk(bp);
    release_buf(bp);
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not read
// it from disk.
struct inode *
iget(uint dev, uint inum) {
    debugcore("iget");

    struct inode *inode_ptr, *empty;
    acquire(&itable.lock);
    // Is the inode already in the table?
    empty = nullptr;
    for (inode_ptr = &itable.inode[0]; inode_ptr < &itable.inode[NINODE]; inode_ptr++) {
        if (inode_ptr->ref > 0 && inode_ptr->dev == dev && inode_ptr->inum == inum) {
            inode_ptr->ref++;
            release(&itable.lock);
            return inode_ptr;
        }
        if (empty == 0 && inode_ptr->ref == 0) // Remember empty slot.
            empty = inode_ptr;
    }

    // Recycle an inode entry.
    if (empty == nullptr)
        panic("iget: no inodes");

    inode_ptr = empty;
    inode_ptr->dev = dev;
    inode_ptr->inum = inum;
    inode_ptr->ref = 1;
    inode_ptr->valid = 0;
    release(&itable.lock);
    return inode_ptr;
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode table entry can
// be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput(struct inode *ip) {
    tracecore("iput");
    acquire(&itable.lock);

    if (ip->ref == 1 && ip->valid && ip->num_link == 0) {
        // inode has no links and no other references: truncate and free.

        // ip->ref == 1 means no other process can have ip locked,
        // so this acquiresleep() won't block (or deadlock).
        acquire_mutex_sleep(&ip->lock);

        release(&itable.lock);

        itrunc(ip);
        ip->type = 0;
        iupdate(ip);
        ip->valid = 0;

        release_mutex_sleep(&ip->lock);

        acquire(&itable.lock);
    }

    ip->ref--;
    release(&itable.lock);
}

// Increment reference count for ip.
// Returns ip to enable ip = idup(ip1) idiom.
struct inode *
idup(struct inode *ip) {
    kernel_assert(ip != nullptr, "inode can not be nullptr");
    acquire(&itable.lock);
    ip->ref++;
    release(&itable.lock);
    return ip;
}



// Common idiom: unlock, then put.
void iunlockput(struct inode *ip) {
    iunlock(ip);
    iput(ip);
}



// Reads the inode from disk if necessary.
void ivalid(struct inode *ip) {
    debugcore("ivalid");

    struct buf *bp;
    struct dinode *dip;
    if (ip->valid == 0) {
        bp = acquire_buf_and_read(ip->dev, BLOCK_CONTAINING_INODE(ip->inum, sb));
        dip = (struct dinode *)bp->data + ip->inum % INODES_PER_BLOCK;
        ip->type = dip->type;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        release_buf(bp);
        ip->valid = 1;
        if (ip->type == 0)
            panic("ivalid: no type");
    }
}




// Read data from inode.
// If user_dst==1, then dst is a user virtual address;
// otherwise, dst is a kernel address.
int readi(struct inode *ip, int user_dst, void *dst, uint off, uint n) {
    // debugcore("readi");
    uint tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return 0;
    if (off + n > ip->size)
        n = ip->size - off;

    for (tot = 0; tot < n; tot += m, off += m, dst += m) {
        bp = acquire_buf_and_read(ip->dev, bmap(ip, off / BSIZE));
        m = MIN(n - tot, BSIZE - off % BSIZE);
        if (either_copyout((char *)dst, (char *)bp->data + (off % BSIZE), m, user_dst) == -1) {
            release_buf(bp);
            tot = -1;
            break;
        }
        release_buf(bp);
    }
    return tot;
}

// Write data to inode.
// Caller must hold ip->lock.
// If user_src==1, then src is a user virtual address;
// otherwise, src is a kernel address.
// Returns the number of bytes successfully written.
// If the return value is less than the requested n,
// there was an error of some kind.
int writei(struct inode *ip, int user_src, void *src, uint off, uint n) {
    uint tot, m;
    struct buf *bp;

    if (off > ip->size || off + n < off)
        return -1;
    if (off + n > MAXFILE * BSIZE)
        return -1;

    for (tot = 0; tot < n; tot += m, off += m, src += m) {
        bp = acquire_buf_and_read(ip->dev, bmap(ip, off / BSIZE));
        m = MIN(n - tot, BSIZE - off % BSIZE);
        if (either_copyin((char *)bp->data + (off % BSIZE), (char *)src, m, user_src) == -1) {
            release_buf(bp);
            break;
        }
        write_buf_to_disk(bp);
        release_buf(bp);
    }

    if (off > ip->size)
        ip->size = off;

    // write the i-node back to disk even if the size didn't change
    // because the loop above might have called bmap() and added a new
    // block to ip->addrs[].
    iupdate(ip);

    return tot;
}



// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock(struct inode *ip) {
    struct buf *bp;
    struct dinode *dip;

    if (ip == 0 || ip->ref < 1)
        panic("ilock");

    acquire_mutex_sleep(&ip->lock);

    if (ip->valid == 0) {
        bp = acquire_buf_and_read(ip->dev, BLOCK_CONTAINING_INODE(ip->inum, sb));
        dip = (struct dinode *)bp->data + ip->inum % INODES_PER_BLOCK;
        ip->type = dip->type;
        ip->major = dip->major;
        ip->minor = dip->minor;
        ip->num_link = dip->num_link;
        ip->size = dip->size;
        memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
        release_buf(bp);
        ip->valid = 1;
        if (ip->type == 0) {
            errorf("dev=%d, inum=%d", (int)ip->dev, (int)ip->inum);
            panic("ilock: no type, this disk inode is invalid");
        }
    }
}

// Unlock the given inode.
void iunlock(struct inode *ip) {
    if (ip == nullptr || !holdingsleep(&ip->lock) || ip->ref < 1)
        panic("iunlock");

    release_mutex_sleep(&ip->lock);
}




struct inode *
inode_by_name(char *path) {
    char name[DIRSIZ];
    return inode_or_parent_by_name(path, 0, name);
}

struct inode *
inode_parent_by_name(char *path, char *name) {
    return inode_or_parent_by_name(path, 1, name);
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRSIZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode *
inode_or_parent_by_name(char *path, int nameiparent, char *name) {
    struct inode *ip, *next;
    debugcore("inode_or_parent_by_name");
    if (*path == '/') {
        // absolute path
        ip = iget(ROOTDEV, ROOTINO);
    } else {
        // relative path
        ip = idup(curr_proc()->cwd);
    }

    while ((path = skipelem(path, name)) != 0) {
        ilock(ip);
        if (ip->type != T_DIR) {
            iunlockput(ip);
            return 0;
        }
        if (nameiparent && *path == '\0') {
            // Stop one level early.
            iunlock(ip);
            return ip;
        }
        if ((next = dirlookup(ip, name, 0)) == 0) {
            iunlockput(ip);
            return 0;
        }
        iunlockput(ip);
        ip = next;
    }
    if (nameiparent) {
        iput(ip);
        return 0;
    }
    return ip;
}

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[].  The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint
bmap(struct inode *ip, uint bn) {
    uint addr, *a;
    struct buf *bp;

    if (bn < NDIRECT) {
        if ((addr = ip->addrs[bn]) == 0)
            ip->addrs[bn] = addr = alloc_zeroed_block(ip->dev);
        return addr;
    }
    bn -= NDIRECT;

    if (bn < NINDIRECT) {
        // Load indirect block, allocating if necessary.
        if ((addr = ip->addrs[NDIRECT]) == 0)
            ip->addrs[NDIRECT] = addr = alloc_zeroed_block(ip->dev);
        bp = acquire_buf_and_read(ip->dev, addr);
        a = (uint *)bp->data;
        if ((addr = a[bn]) == 0) {
            a[bn] = addr = alloc_zeroed_block(ip->dev);
            write_buf_to_disk(bp);
        }
        release_buf(bp);
        return addr;
    }

    panic("bmap: out of range");
    return 0;
}

// Truncate inode (discard contents).
// Caller must hold ip->lock.
void itrunc(struct inode *ip) {
    tracecore("itrunc");
    int i, j;
    struct buf *bp;
    uint *a;

    for (i = 0; i < NDIRECT; i++) {
        if (ip->addrs[i]) {
            free_disk_block(ip->dev, ip->addrs[i]);
            ip->addrs[i] = 0;
        }
    }

    if (ip->addrs[NDIRECT]) {
        bp = acquire_buf_and_read(ip->dev, ip->addrs[NDIRECT]);
        a = (uint *)bp->data;
        for (j = 0; j < NINDIRECT; j++) {
            if (a[j])
                free_disk_block(ip->dev, a[j]);
        }
        release_buf(bp);
        free_disk_block(ip->dev, ip->addrs[NDIRECT]);
        ip->addrs[NDIRECT] = 0;
    }

    ip->size = 0;
    iupdate(ip);
}





// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff)
{
    uint off, inum;
    struct dirent de;

    if (dp->type != T_DIR)
        panic("dirlookup not DIR");

    for (off = 0; off < dp->size; off += sizeof(de))
    {
        if (readi(dp, 0, &de, off, sizeof(de)) != sizeof(de))
            panic("dirlookup read");
        if (de.inum == 0)
            continue;
        if (namecmp(name, de.name) == 0)
        {
            // entry matches path element
            if (poff)
                *poff = off;
            inum = de.inum;
            return iget(dp->dev, inum);
        }
    }

    return 0;
}

// Write a new directory entry (name, inum) into the directory dp.
int dirlink(struct inode *dp, char *name, uint inum)
{
    int off;
    struct dirent de;
    struct inode *ip;
    // Check that name is not present.
    if ((ip = dirlookup(dp, name, 0)) != 0)
    {
        iput(ip);
        return -1;
    }

    // Look for an empty dirent.
    for (off = 0; off < dp->size; off += sizeof(de))
    {
        if (readi(dp, false, &de, off, sizeof(de)) != sizeof(de))
            panic("dirlink read");
        if (de.inum == 0)
            break;
    }
    strncpy(de.name, name, DIRSIZ);
    de.inum = inum;
    if (writei(dp, false, &de, off, sizeof(de)) != sizeof(de))
        panic("dirlink");
    return 0;
}


// Copy stat information from inode.
// Caller must hold ip->lock.
void stati(struct inode *ip, struct stat *st)
{
    st->dev = ip->dev;
    st->ino = ip->inum;
    st->type = ip->type;
    st->nlink = ip->num_link;
    st->size = ip->size;
}
// Is the directory dp empty except for "." and ".." ?
int isdirempty(struct inode *dp)
{
    int off;
    struct dirent de;

    for (off = 2 * sizeof(de); off < dp->size; off += sizeof(de))
    {
        if (readi(dp, 0, (void *)&de, off, sizeof(de)) != sizeof(de))
            panic("isdirempty: readi");
        if (de.inum != 0)
            return 0;
    }
    return 1;
}
*/