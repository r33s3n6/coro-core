#include <device/block/bdev.h>
#include <device/block/buf.h>

#include <mm/utils.h>
#include <utils/assert.h>

#include <utils/log.h>
#include <task_scheduler.h>

#include "inode.h"


namespace nfs {

uint32 nfs_inode::cache_hit = 0;
uint32 nfs_inode::cache_miss = 0;


nfs_inode::~nfs_inode() {
    // ((nfs*)fs)->put_inode(inode_number);
}

void nfs_inode::on_destroy(weak_ptr<nfs_inode> self) {
    {
        auto ptr = self.lock();
        if (ptr && ptr->flush_needed()) {
            debugf("nfs_inode::on_destroy: %d", ptr->inode_number);
            kernel_task_scheduler[0].schedule(std::move(((nfs*)ptr->fs)->put_inode(ptr)));
        }
    }
    
}

void nfs_inode::print(){
    infof(
    "nfs_inode::print\n"
    "inode_number: %d\n"
    "fs: %p\n"
    "perm: %d\n"
    "nlinks: %d\n"
    "size: %d\n"
    "type: %d\n"
    "addrs[0]: %d\n"
    "next_addr_block: %d"
    , inode_number, fs, metadata.perm, metadata.nlinks, metadata.size, metadata.type, addrs[0], next_addr_block);
}

template <bool _write>
task<void> nfs_inode::direct_data_block_rw(
    uint32 direct_offset, uint32 data_block_offset,
    std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size) { 
    
    auto bdev = (block_device*)(this->fs->get_device());

    kernel_assert(direct_offset < NUM_DIRECT_DATA, "direct_offset out of range");
    kernel_assert(data_block_offset + size <= block_device::BLOCK_SIZE, "size out of range");

    uint32 data_block_index = addrs[direct_offset];

    // debugf("inode %d: get data block index: %d from direct offset: %d",inode_number, data_block_index, direct_offset);

    bool clear = false;

    if(data_block_index == (uint32)-1){
        if constexpr (_write){
            
            // allocate a new block
            data_block_index = *co_await  ((nfs*)fs)->alloc_block();
            addrs[direct_offset] = data_block_index;
            mark_dirty();
            clear = true;
        } else {
            // all zero block
            memset(buf, 0, size);
            co_return task_ok;
        }
    }

    auto block_buf_ptr = *co_await kernel_block_buffer.get(bdev, data_block_index);
    auto block_buf_ref = *co_await block_buf_ptr->get_ref();
    auto block_buf = block_buf_ref->data;

    if constexpr (_write) {
        memcpy(block_buf + data_block_offset, buf, size);
        if(clear) {
            memset(block_buf, 0, data_block_offset);
            memset(block_buf + data_block_offset + size, 0, block_device::BLOCK_SIZE - data_block_offset - size);
        }
        block_buf_ref->mark_dirty();
    } else {
        memcpy(buf, block_buf + data_block_offset, size);
    }

    co_return task_ok;
}

template <bool _write>
task<void> nfs_inode::data_block_rw(
    uint32 addr_block_index, uint32 addr_block_offset, uint32 data_block_offset,
    std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size) { 
    
    auto bdev = (block_device*)(this->fs->get_device());
    
    kernel_assert(addr_block_offset < NUM_ADDRS, "addr_block_offset out of range");
    kernel_assert(data_block_offset + size <= block_device::BLOCK_SIZE, "size out of range");

    uint32 data_block_index;
    bool clear = false;

    {
        // read addr block
        auto addr_block_buf_ptr = *co_await kernel_block_buffer.get(bdev, addr_block_index);
        auto addr_block_buf_ref = *co_await addr_block_buf_ptr->get_ref();
        auto addr_block_buf = addr_block_buf_ref->data;

        addr_block *_addr_block = (addr_block*)addr_block_buf;

        data_block_index = _addr_block->addrs[addr_block_offset];

        if(data_block_index == (uint32)-1){
            if constexpr (_write){
                // allocate a new block
                data_block_index = *co_await  ((nfs*)fs)->alloc_block();
                _addr_block->addrs[addr_block_offset] = data_block_index;
                addr_block_buf_ref->mark_dirty();
                clear = true;
            } else {
                // all zero block
                memset(buf, 0, size);
                co_return task_ok;
            }
        }
    }

    auto block_buf_ptr = *co_await kernel_block_buffer.get(bdev, data_block_index);
    auto block_buf_ref = *co_await block_buf_ptr->get_ref();
    auto block_buf = block_buf_ref->data;

    
    if constexpr (_write) {
        memcpy(block_buf + data_block_offset, buf, size);
        if(clear) {
            memset(block_buf, 0, data_block_offset);
            memset(block_buf + data_block_offset + size, 0, block_device::BLOCK_SIZE - data_block_offset - size);
        }
        block_buf_ref->mark_dirty();
    } else {
        memcpy(buf, block_buf + data_block_offset, size);
    }



    co_return task_ok;

}

template <bool _write>
task<int64> nfs_inode::data_rw(std::conditional_t<_write, const uint8 *, uint8*> buf, uint64 offset, uint64 size) {
    auto bdev = (block_device*)(this->fs->get_device());

    int64 rw_size = 0;
    uint32 block_rw_size = 0;

    uint32 addr_block_index = 0;
    uint32 addr_block_offset = 0;
    // uint32 data_block_index = 0;
    uint32 data_block_offset = 0;


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
            next_addr_block = *co_await ((nfs*)fs)->alloc_block();
            mark_dirty();

            // init next_addr_block with -1
            auto next_addr_block_buf_ptr = *co_await kernel_block_buffer.get(bdev, next_addr_block);
            auto next_addr_block_buf_ref = *co_await next_addr_block_buf_ptr->get_ref();
            auto next_addr_block_buf = next_addr_block_buf_ref->data;

            memset(next_addr_block_buf, 0xff, block_device::BLOCK_SIZE);
            next_addr_block_buf_ref->mark_dirty();
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
    // make metadata valid
    co_await load();

    co_return *co_await data_rw<false>((uint8*)dest, offset, size);

}
task<int64> nfs_inode::write(const void *src, uint64 offset, uint64 size) {
    // make metadata valid
    co_await load();

    int64 write_size = *co_await data_rw<true>((const uint8*)src, offset, size);
    if (offset + write_size > metadata.size) {
        metadata.size = offset + write_size;
        this->mark_dirty();
    }
    co_return write_size;
}

task<const nfs_inode::metadata_t*> nfs_inode::get_metadata() {
    // if (!metadata_valid) {
    //     // read inode information from disk
    //     co_await load();
    // }

    co_return &metadata;
}

task<int32> nfs_inode::truncate(int64 size) {
    auto bdev = (block_device*)(fs->get_device());
    // resize inode
    
    // TODO: only support truncate to 0 now
    if (size != 0) {
        co_return -1;
    }

    // make metadata valid
    co_await load();

    if (metadata.size == 0) {
        co_return 0;
    }

    // free all direct data blocks
    for (uint32 i = 0; i < NUM_DIRECT_DATA; i++) {
        if (addrs[i] != (uint32)-1) {
            co_await ((nfs*)fs)->free_block(addrs[i]);
            addrs[i] = (uint32)-1;
        }
    }

    uint32 addr_block_index = next_addr_block;
   
    uint32 data_block_index = 0;

    // free all indirect data blocks
    while (addr_block_index != (uint32)-1) {
        uint32 next_addr_block_index;
        {
            auto addr_block_buf_ptr = *co_await kernel_block_buffer.get(bdev, addr_block_index);
            auto addr_block_buf_ref = *co_await addr_block_buf_ptr->get_ref();

            addr_block* _addr_block = (addr_block*)(addr_block_buf_ref->data);
            for (uint32 i = 0; i < NUM_ADDRS; i++) {
                data_block_index = _addr_block->addrs[i];
                if (data_block_index != (uint32)-1) {
                    co_await ((nfs*)fs)->free_block(data_block_index);
                }
            }
            next_addr_block_index = _addr_block->next_addr_block;
        }

        co_await ((nfs*)fs)->free_block(addr_block_index);
        addr_block_index = next_addr_block_index;
    }
    
    next_addr_block = (uint32)-1;
    metadata.size = 0;
    this->mark_dirty();

    co_return 0;
}

task<int32> nfs_inode::__link(shared_ptr<dentry> new_dentry, shared_ptr<nfs_inode> _inode) {
    // TODO: performance issue

    dirent temp_dirent;
    dirent new_dirent;
    uint64 offset = 0;

    {
        auto inode_ref = *co_await _inode->get_ref();

        new_dirent.inode_number = inode_ref->inode_number;
        strncpy(new_dirent.name, new_dentry->name.data(), new_dentry->name.size());
        new_dirent.name[new_dentry->name.size()] = 0;

        inode_ref->set_dentry(new_dentry);
        new_dentry->set_inode(_inode);


        inode_ref->metadata.nlinks += 1;
        inode_ref->mark_dirty();
    }



    // find empty slot
    while (true) {
        if (offset >= metadata.size) {
            break;
        }

        int64 read_size = *co_await read(&temp_dirent, offset, sizeof(dirent));
        if (read_size != sizeof(dirent)) {
            warnf("read dirent failed");
            co_return -1;
        }
        
        if (temp_dirent.inode_number == 0) {
            break;
        }

        offset += sizeof(dirent);
    }

    co_await write(&new_dirent, offset, sizeof(dirent));

    co_return 0;
}


task<int32> nfs_inode::create(shared_ptr<dentry> new_dentry) {

    if (new_dentry->name.size() > MAX_NAME_LENGTH) {
        co_return -EINVAL;
    }

    // make metadata valid
    co_await load();
    

    auto new_inode = *co_await ((nfs*)fs)->alloc_inode();
    new_inode->print();

    {
        auto inode_ref = *co_await new_inode->get_ref();

        inode_ref->metadata.type = inode::ITYPE_FILE;
        inode_ref->mark_dirty();

    }

    int32 ret = *co_await __link(new_dentry, new_inode);
    co_return ret;
}

task<int32> nfs_inode::lookup(shared_ptr<dentry> new_dentry) {

    if (new_dentry->name.size() > MAX_NAME_LENGTH) {
        co_return -EINVAL;
    }

    uint32 offset = 0;
    dirent _dirent;

    // make metadata valid
    co_await load();
    
    while (true) {
        if (offset >= metadata.size) {
            warnf("not found");
            co_return -1;
        }
        int64 read_size = *co_await read(&_dirent, offset, sizeof(dirent));
        if (read_size != sizeof(dirent)) {
            warnf("read dirent failed");
            co_return -1;
        }
        offset += sizeof(dirent);
        if (_dirent.inode_number == 0) {
            continue;
        }

        if (strcmp(_dirent.name, new_dentry->name.data()) == 0) {
            // found
            auto new_inode = *co_await ((nfs*)fs)->get_inode(_dirent.inode_number);
            auto inode_ref = *co_await new_inode->get_ref();
            inode_ref->set_dentry(new_dentry);
            new_dentry->set_inode(new_inode);

            co_return 0;
        }

    }
}
// generator
task<shared_ptr<dentry>> nfs_inode::read_dir() {

    // make metadata valid
    co_await load();

    shared_ptr<dentry> this_dentry = this->this_dentry.lock();

    kernel_assert(this_dentry->get_inode().get() == this, "parent inode mismatch");

    uint32 offset = 0;
    dirent _dirent;


    
    
    while (true) {
        if (offset >= metadata.size) {
            co_return nullptr;
        }

        int64 read_size = *co_await read(&_dirent, offset, sizeof(dirent));
        if (read_size != sizeof(dirent)) {
            warnf("read dirent failed");
            co_return nullptr;
        }
        offset += sizeof(dirent);
        if (_dirent.inode_number == 0) {
            continue;
        }

        {
            auto new_inode = *co_await ((nfs*)fs)->get_inode(_dirent.inode_number);
            shared_ptr<dentry> d = nullptr;
            d = *co_await kernel_dentry_cache.get_or_create(this_dentry, _dirent.name, new_inode);
            co_yield d;
        }

        
    }
}


task<int32> nfs_inode::link(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry) {
    if (new_dentry->name.size() > MAX_NAME_LENGTH) {
        co_return -EINVAL;
    }

    // if (!old_dentry->_inode) {
    //     co_await lookup(old_dentry);
    // }

    // make metadata valid
    co_await load();

    shared_ptr<nfs_inode> old_inode = (shared_ptr<nfs_inode>)old_dentry->get_inode();

    co_return *co_await __link(new_dentry, old_inode);

}
task<int32> nfs_inode::symlink(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry) {
    (void)(old_dentry);
    (void)(new_dentry);
    co_return -EPERM;
}
task<int32> nfs_inode::unlink(shared_ptr<dentry> old_dentry) {
    if (old_dentry->name.size() > MAX_NAME_LENGTH) {
        co_return -EINVAL;
    }

    dirent _dirent;
    uint32 offset = 0;

    // make metadata valid
    co_await load();

    //debugf("unlink '%s'", old_dentry->name.data());

    while (true) {
        if (offset >= metadata.size) {
            co_return -1;
        }

        int64 read_size = *co_await read(&_dirent, offset, sizeof(dirent));
        if (read_size != sizeof(dirent)) {
            warnf("read dirent failed");
            co_return -1;
        }
        
        if (_dirent.inode_number != 0) {
            if (strcmp(_dirent.name, old_dentry->name.data()) == 0) {
                // found

                
                auto old_inode = (shared_ptr<nfs_inode>)old_dentry->get_inode();

                {
                    auto inode_ref = *co_await old_inode->get_ref();

                    //debugf("unlink(inner) '%s' (%d)", old_dentry->name.data(), inode_ref->inode_number);
                    
                    if (--inode_ref->metadata.nlinks == 0){
                        co_await inode_ref->truncate(0);
                        // inode_ref->mark_dirty();

                        co_await inode_ref->flush();
                        // TODO: free inode
                        co_await ((nfs*)fs)->free_inode(inode_ref->inode_number);
                        inode_ref->mark_invalid();
                        inode_ref->mark_clean();

                    }else{
                        inode_ref->mark_dirty();
                    }
                }

                _dirent.inode_number = 0;
                co_await write(&_dirent, offset, sizeof(dirent));

                co_return 0;
            }
        }

        offset += sizeof(dirent);

        

    }
}


task<int32> nfs_inode::mkdir(shared_ptr<dentry> new_dentry) {
    // validate name
    if (new_dentry->name.size() > MAX_NAME_LENGTH) {
        co_return -EINVAL;
    }


    // make metadata valid
    co_await load();
    

    auto new_inode = *co_await ((nfs*)fs)->alloc_inode();
    {
        auto inode_ref = *co_await new_inode->get_ref();

        inode_ref->metadata.type = inode::ITYPE_DIR;
        inode_ref->mark_dirty();
    }

    int32 ret = *co_await __link(new_dentry, new_inode);



    co_return ret;
}

// 
task<int32> nfs_inode::rmdir(shared_ptr<dentry> old_dentry) {
   (void)(old_dentry);
    co_return -EPERM;
}

void nfs_inode::init_data() {

    metadata.size = 0;
    metadata.perm = 0;
    metadata.type = 0;
    metadata.nlinks = 0;

    memset(addrs, 0xff, sizeof(addrs));

    next_addr_block = (uint32)-1;

    this->mark_valid();
    this->mark_dirty();
}

task<int32> nfs_inode::__load() {

    dinode dinode_buf;
    dinode* disk_inode;
    if (inode_number == INODE_TABLE_NUMBER) {
        // inode table, we read it from superblock
        disk_inode = &((nfs*)fs)->sb.inode_table;
    } else {
        auto inode_table_ref = *co_await ((nfs*)fs)->inode_table->get_ref();
        co_await inode_table_ref->read(&dinode_buf, inode_number * sizeof(dinode), sizeof(dinode));
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

    co_return 0;
}
task<int32> nfs_inode::__flush() {

    dinode dinode_buf;
    dinode* disk_inode;

    if (inode_number == INODE_TABLE_NUMBER) {
        // inode table, we write it to superblock
        disk_inode = &((nfs*)fs)->sb.inode_table;
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
        ((nfs*)fs)->sb_dirty = true;
    } else {
        // debugf("flush inode %d", inode_number);
        // print();
        auto inode_table_ref = *co_await ((nfs*)fs)->inode_table->get_ref();
        co_await inode_table_ref->write(disk_inode, inode_number * sizeof(dinode), sizeof(dinode));
    }


    co_return 0;
}

// create intermediate addr blocks if necessary
task<void> nfs_inode::get_block_index(uint32 start_addr_block, uint64 offset,
         uint32* addr_block_index, uint32* addr_block_offset, uint32* data_block_index, uint32* data_block_offset) {
    
    auto bdev = (block_device*)fs->get_device();

    uint32 _addr_block_index = start_addr_block;

    block_buffer_t::buffer_ptr_t buf_ptr;

    // find final index block
    while (true) {
        buf_ptr = *co_await kernel_block_buffer.get(bdev, _addr_block_index);

        if (offset < ADDR_BLOCK_DATA_SIZE) {
            break;
        }

        auto buf_ref = *co_await buf_ptr->get_ref();

        const addr_block* buf = (const addr_block*)(buf_ref->data);

        if (buf->next_addr_block == (uint32)-1) { 
            break;
        }

        offset -= ADDR_BLOCK_DATA_SIZE;
        _addr_block_index = buf->next_addr_block;
    }

    // create all intermediate addr blocks
    while (offset >= ADDR_BLOCK_DATA_SIZE) {
        // buf_ptr hold the last addr block
        

        uint32 new_addr_block_index = *co_await ((nfs*)fs)->alloc_block();
        // debugf("nfs_inode::get_block_index: alloc new addr block %d", new_addr_block_index);
        auto new_buf_ptr = *co_await kernel_block_buffer.get(bdev, new_addr_block_index);

        {
            auto new_buf_ref = *co_await new_buf_ptr->get_ref();
            memset(new_buf_ref->data, 0xff, BLOCK_SIZE);
            new_buf_ref->mark_dirty();
        }

        // update previous addr block
        {
            auto buf_ref = *co_await buf_ptr->get_ref();
            addr_block* buf = (addr_block*)(buf_ref->data);
            buf->next_addr_block = new_addr_block_index;
            buf_ref->mark_dirty();
        }

        _addr_block_index = new_addr_block_index;
        buf_ptr = std::move(new_buf_ptr);

        offset -= ADDR_BLOCK_DATA_SIZE;

        // debugf("nfs_inode::get_block_index: create intermediate addr block: %d", _addr_block_index);

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
        auto buf_ref = *co_await buf_ptr->get_ref();
        const addr_block* buf = (const addr_block*)(buf_ref->data);
        uint32 _data_block_index = buf->addrs[_addr_block_offset];

        *data_block_index = _data_block_index;
    }
    if(data_block_offset) {
        *data_block_offset = offset % BLOCK_SIZE;
    }

    co_return task_ok;
}

}


