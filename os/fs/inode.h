#ifndef FS_INODE_H
#define FS_INODE_H

#include "dentry.h"

#include <ccore/types.h>
#include <ccore/errno.h>

#include <coroutine.h>
#include <atomic/lock.h>
#include <atomic/mutex.h>

#include <utils/utility.h>
#include <utils/list.h>

#include <device/device.h>
#include <utils/buffer.h>

#include <utils/shared_ptr.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class filesystem;

// coroutine inode
class inode : public referenceable_buffer<inode> {
    public:
    enum inode_type : uint8 {
        ITYPE_NONE = 0,
        ITYPE_FILE = 1,
        ITYPE_DIR,
        ITYPE_CHAR,
        ITYPE_BLOCK,
        ITYPE_FIFO,
        ITYPE_SOCKET,
        ITYPE_SYMLINK,
        ITYPE_NEXT_FREE_INODE
    };


    struct metadata_t {
        uint32 size;
        uint16 nlinks;
        uint8 perm;
        uint8 type;
    };

    inode(){}
    inode(filesystem* _fs, uint64 _inode_number) : fs(_fs), inode_number(_inode_number) {}
    virtual ~inode() {}
    
    // inode operations
    // normally, these operations are called for file operations
    virtual task<int32> truncate(int64 size)  { co_return -EPERM; };
    virtual task<int64> read(void *buf, uint64 offset, uint64 size)  { co_return -EPERM; };
    virtual task<int64> write(const void *buf, uint64 offset, uint64 size)  { co_return -EPERM; };
    virtual task<const metadata_t*> get_metadata()  { co_return nullptr; };
    
    // we assume current inode is a directory
    virtual task<int32> create(shared_ptr<dentry> new_dentry)  { co_return -EPERM; };
    virtual task<int32> lookup(shared_ptr<dentry> new_dentry) { co_return -EPERM; };

    // generator
    virtual task<shared_ptr<dentry>> read_dir() { co_return nullptr; };

    virtual task<int32> link(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry)  { co_return -EPERM; };
    virtual task<int32> symlink(shared_ptr<dentry> old_dentry, shared_ptr<dentry> new_dentry)  { co_return -EPERM; };
    virtual task<int32> unlink(shared_ptr<dentry> old_dentry )  { co_return -EPERM; };
    
    virtual task<int32> mkdir(shared_ptr<dentry> new_dentry)  { co_return -EPERM; };
    virtual task<int32> rmdir(shared_ptr<dentry> old_dentry)  { co_return -EPERM; };

    // sync from disk to memory
    virtual task<int32> __load()  { co_return -EPERM; };
    virtual task<int32> __flush()  { co_return -EPERM; };

    

    weak_ptr<dentry> get_dentry();
    void set_dentry(shared_ptr<dentry> _dentry);

    void init(filesystem* _fs=nullptr, uint32 _inode_number=0) {
        fs = _fs;
        inode_number = _inode_number;
        this->mark_invalid();
    }

    bool match(filesystem* _fs, uint32 _inode_number) {
        return this->fs == _fs && (this->inode_number == _inode_number || _inode_number == (uint32)(-1));
    }

    void print();



public:
    filesystem* fs = nullptr;
    uint32 inode_number = 0;
    
    // metadata
public:

    metadata_t metadata;

private:
friend class dentry;


    // template <std::derived_from<inode> T>
    // friend class T;

    protected:
    // weak reference to dentry
    weak_ptr<dentry> this_dentry = {};

};




#pragma GCC diagnostic pop


#endif // FS_INODE_H
