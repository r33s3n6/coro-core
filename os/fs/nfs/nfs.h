#ifndef NFS_FS_H
#define NFS_FS_H

#include "common.h"

#include <fs/fs.h>
#include <utils/list.h>


namespace nfs {

class nfs_inode;

class nfs : public filesystem {
    public:
    virtual ~nfs() = default;
    virtual task<void> mount(device_id_t _device_id) override;
    virtual task<void> unmount() override;

    virtual task<inode*> get_root() override;

    task<uint32> alloc_block();
    task<void> free_block(uint32 block_index);

    nfs_inode* get_inode(uint32 inode_number);
    task<void> put_inode(nfs_inode *inode);

    static task<void> make_fs(device_id_t device_id, uint32 nblocks);

    void print();

    // TODO: private:
    public:
    friend class nfs_inode;
    superblock sb;
    bool sb_dirty = false;
    nfs_inode *root_inode = nullptr;
    nfs_inode *inode_table = nullptr; // inode table inode
    list<nfs_inode*> inode_list;
    spinlock lock { "nfs.lock" };
    bool unmounted = true;

    uint32 no_ref_count = 0;
    single_wait_queue no_ref_wait_queue;
};

}





#endif