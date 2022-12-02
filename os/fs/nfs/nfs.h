#ifndef NFS_FS_H
#define NFS_FS_H

#include "common.h"

#include <fs/fs.h>
#include <utils/list.h>

#include <utils/buffer_manager.h>

namespace nfs {

class nfs_inode;

class nfs : public filesystem {
    public:

    using inode_t = nfs_inode;
    using inode_ptr_t = shared_ptr<inode_t>;

    virtual ~nfs() = default;
    virtual task<void> mount(device_id_t _device_id) override;
    virtual task<void> unmount() override;

    virtual task<shared_ptr<inode>> get_root() override;

    task<uint32> alloc_block();
    task<void> free_block(uint32 block_index);

    task<inode_ptr_t> alloc_inode();
    task<void> free_inode(uint32 inode_index);

    task<inode_ptr_t> get_inode(uint32 inode_number);
    void __put_inode(uint32 inode_number);
    task<void> put_inode(inode_ptr_t inode_ptr);

    static task<void> make_fs(device_id_t device_id, uint32 nblocks);

    void print();

    // TODO: private:
    public:
    friend class nfs_inode;

    superblock sb;
    bool sb_dirty = false;

    shared_ptr<nfs_inode> root_inode = nullptr;
    shared_ptr<nfs_inode> inode_table = nullptr; // inode table inode
    

    spinlock lock { "nfs.lock" };
    bool unmounted = true;

    int32 no_ref_count = 0;
    int32 wait_count = -1;
    single_wait_queue no_ref_wait_queue;
};

}





#endif