#ifndef NFS_INODE_H
#define NFS_INODE_H

#include <fs/inode.h>
#include "nfs.h"

namespace nfs {

class nfs_inode : public inode {
    public:
    nfs_inode(nfs* _fs, uint64 _inode_number) : inode(_fs->get_device_id(), _inode_number), _fs(_fs) {}

    virtual ~nfs_inode();

    // inode operations
    // normally, these operations are called for file operations
    virtual task<int32> truncate(int64 size) override;
    virtual task<int64> read(void *dest, uint64 offset, uint64 size) override;
    virtual task<int64> write(const void *src, uint64 offset, uint64 size) override;
    virtual task<const metadata_t*> get_metadata() override;
    
    // we assume current inode is a directory
    virtual task<int32> create(dentry* new_dentry) override;
    virtual task<int32> lookup(dentry* new_dentry) override;
    // generator
    virtual task<dentry*> read_dir() override;

    virtual task<int32> link(dentry* old_dentry, dentry* new_dentry) override;
    virtual task<int32> symlink(dentry* old_dentry, dentry* new_dentry) override;
    virtual task<int32> unlink(dentry* old_dentry ) override;
    virtual task<int32> mkdir(dentry* new_dentry) override;
    virtual task<int32> rmdir(dentry* old_dentry) override;

    // sync from disk to memory
    virtual task<int32> load() override;
    virtual task<int32> flush() override;
    
    virtual task<void> put() override;

    void init();
    void print();


    private:

    friend class nfs;
    nfs* _fs = nullptr;
    uint32 addrs[NUM_DIRECT_DATA]; // first few blocks index cache
    uint32 next_addr_block; // next block index cache
    // uint32 inode_block_index = -1; // inode block index cache
    // uint32 inode_offset = -1; // inode offset cache

    public:
    static uint32 cache_hit;
    static uint32 cache_miss;
    
    private:
    uint32 cache_raw_indirect_offset = -1;
    uint32 cache_offset = -1;
    uint32 cache_addr_block = -1;




    task<void> get_block_index(uint32 start_addr_block, uint64 offset,
         uint32* addr_block_index, uint32* addr_block_offset, uint32* block_index, uint32* block_offset);

    template <bool _write>
    task<void> data_block_rw(
        uint32 addr_block_index, uint32 addr_block_offset, uint32 data_block_offset,
        std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size);

    template <bool _write>
    task<void> direct_data_block_rw(
        uint32 direct_offset, uint32 data_block_offset,
        std::conditional_t<_write, const uint8 *, uint8*> buf, uint32 size);

    template <bool _write>
    task<int64> data_rw(std::conditional_t<_write, const uint8 *, uint8*> buf, uint64 offset, uint64 size);

    task<int32> __link(dentry* new_dentry, nfs_inode* _inode);
};

}

#endif