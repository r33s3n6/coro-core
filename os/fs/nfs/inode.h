#ifndef NFS_INODE_H
#define NFS_INODE_H

#include <fs/inode.h>
#include "nfs.h"

namespace nfs {

class nfs_inode : public inode {
    public:
    nfs_inode(nfs* _fs = nullptr, uint64 _inode_number = 0) : inode(_fs, _inode_number) {}

    virtual ~nfs_inode();

    // inode operations
    // normally, these operations are called for file operations
    virtual task<int32> truncate(int64 size) override;
    virtual task<int64> read(void *dest, uint64 offset, uint64 size) override;
    virtual task<int64> write(const void *src, uint64 offset, uint64 size) override;
    virtual task<const metadata_t*> get_metadata() override;
    
    // we assume current inode is a directory
    virtual task<int32> create(shared_ptr<dentry> new_dentry) override;
    virtual task<int32> lookup(shared_ptr<dentry> new_dentry) override;
    // generator
    virtual task<shared_ptr<dentry>> read_dir() override;

    virtual task<int32> link(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry) override;
    virtual task<int32> symlink(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry) override;
    virtual task<int32> unlink(shared_ptr<dentry> old_dentry ) override;
    virtual task<int32> mkdir(shared_ptr<dentry> new_dentry) override;
    virtual task<int32> rmdir(shared_ptr<dentry> old_dentry) override;

    // sync from disk to memory
    virtual task<int32> __load() override;
    virtual task<int32> __flush() override;

    static void on_destroy(weak_ptr<nfs_inode> self);
    
    // virtual task<void> put() override;

    // void init();
    void init_data();
    void print();


    private:

    friend class nfs;

    uint32 addrs[NUM_DIRECT_DATA]; // first few blocks index cache
    uint32 next_addr_block; // next block index cache


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

    task<int32> __link(shared_ptr<dentry> new_dentry, shared_ptr<nfs_inode> _inode);
};

}

#endif