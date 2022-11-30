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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class filesystem;

// coroutine inode
class inode : noncopyable {
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
    inode(device_id_t _device_id, uint64 _inode_number, filesystem* _fs) : device_id(_device_id), inode_number(_inode_number), _fs(_fs) {}
    virtual ~inode() {}
    
    // inode operations
    // normally, these operations are called for file operations
    virtual task<int32> truncate(int64 size)  { co_return -EPERM; };
    virtual task<int64> read(void *buf, uint64 offset, uint64 size)  { co_return -EPERM; };
    virtual task<int64> write(const void *buf, uint64 offset, uint64 size)  { co_return -EPERM; };
    virtual task<const metadata_t*> get_metadata()  { co_return nullptr; };
    
    // we assume current inode is a directory
    virtual task<int32> create(dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> lookup(dentry* new_dentry) { co_return -EPERM; };

    // generator
    virtual task<dentry*> read_dir() { co_return nullptr; };

    virtual task<int32> link(dentry* old_dentry, dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> symlink(dentry* old_dentry, dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> unlink(dentry* old_dentry )  { co_return -EPERM; };
    
    virtual task<int32> mkdir(dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> rmdir(dentry* old_dentry)  { co_return -EPERM; };

    // sync from disk to memory
    virtual task<int32> load()  { co_return -EPERM; };
    virtual task<int32> flush()  { co_return -EPERM; };

    virtual void get() {
        inc_ref();
    }
    virtual task<void> put() { dec_ref(); co_return task_ok; };

    task<dentry*> get_dentry();
    task<void> set_dentry(dentry* _dentry);

    uint64 get_ref() {
        auto guard = make_lock_guard(ref_lock);
        return reference_count;
    }

    protected:

    void inc_ref() {
        auto guard = make_lock_guard(ref_lock);
        reference_count++;
    }

    int dec_ref() {
        auto guard = make_lock_guard(ref_lock);
        reference_count--;
        return reference_count;
    }



public:
    // spinlock rw_lock;
    coro_mutex rw_lock {"inode.rw_lock"};
    spinlock ref_lock {"inode.ref_lock"};

public:
    // for kernel inode buffer, and metadata
    int32 reference_count = 0;
    device_id_t device_id;
    uint32 inode_number;
    filesystem* _fs = nullptr;

    // metadata
protected:

    bool metadata_valid = false;
    bool metadata_dirty = false;
    metadata_t metadata;

private:
friend class dentry;
    // weak reference to dentry
    dentry* this_dentry = nullptr;

};

class file {
    public:
    enum class whence_t {
        SET,
        CUR,
        END
    };

    public:
        // file operations
    virtual task<int32> open()  { co_return -EPERM; };
    virtual task<int32> close()  { co_return -EPERM; };
    virtual task<int64> read(void *buf, uint64 size)  { co_return -EPERM; };
    virtual task<int64> write(const void *buf, uint64 size)  { co_return -EPERM; };

    virtual task<int64> llseek(int64 offset, whence_t whence)  { co_return -EPERM; };
    
    // virtual task<int32> flush()  { co_return -EPERM; };

    virtual task<int64> tell()  { co_return -EPERM; };

    file(){}
    virtual ~file() {}



    public:
    struct file_rw_lock_t : noncopyable, nonmovable {
        file* f;
        file_rw_lock_t(file* f) : f(f) {
        }

        void lock_off() {
            f->in_rw = true;
        }

        void lock_on() {
            f->in_rw = false;
        }

        task<void> lock() {
            co_await f->__file_rw_lock.lock();
            f->in_rw = true;
        }

        void unlock() {
            f->in_rw = false;
            f->__file_rw_lock.unlock();
        }
    };

    file_rw_lock_t file_rw_lock{this};

    protected:
    struct __file_rw_lock_t : noncopyable, nonmovable {
        file* f;
        __file_rw_lock_t(file* f) : f(f) {
        }
        task<void> lock() {
            if(!f->in_rw){
                co_await f->_inode->rw_lock.lock();
            }
            co_return task_ok;
        }

        void unlock() {
            if(!f->in_rw){
                f->_inode->rw_lock.unlock();
            }
        }

    };

    __file_rw_lock_t __file_rw_lock{this};


    protected:
    dentry *parent = nullptr; // parent dentry
    inode *_inode = nullptr; // related inode
    uint64 offset = 0;

    private:
    bool in_rw = false;
};

class simple_file : public file {
    public:

    simple_file(inode* _inode){
       this->_inode = _inode;
    }

    ~simple_file() {
        if (opened) {
            panic("simple_file is not closed");
        }
    }

    task<int32> open() override {
        _inode->get();
        offset = 0;
        opened = true;
        co_return 0;
    }

    task<int32> close() override {
        co_await _inode->put();
        _inode = nullptr;
        opened = false;
        co_return 0;
    }

    task<int64> read(void *buf, uint64 size) override {
        co_await __file_rw_lock.lock();
        auto ret = *co_await _inode->read(buf, offset, size);
        __file_rw_lock.unlock();
        if(ret > 0){
            offset += ret;
        }
        co_return ret;
    }

    task<int64> write(const void *buf, uint64 size) override {
        co_await __file_rw_lock.lock();
        auto ret = *co_await _inode->write(buf, offset, size);
        __file_rw_lock.unlock();
        if(ret > 0){
            offset += ret;
        }
        co_return ret;
    }

    task<int64> llseek(int64 offset, whence_t whence) override {
        switch(whence){
            case whence_t::SET:
                this->offset = offset;
                break;
            case whence_t::CUR:
                this->offset += offset;
                break;
            case whence_t::END: {
                auto ret = co_await _inode->get_metadata();
                uint32 size = (*ret)->size;
                this->offset = size + offset;
                break;
            }
        }
        co_return this->offset;
    }


    task<int64> tell() override {
        co_return offset;
    }

    private:
    bool opened = false;

};


#pragma GCC diagnostic pop

/*

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)

struct inode {
    uint dev;  // Device number
    uint inum; // Inode number
    int ref;   // Reference count
    struct mutex lock;
    int valid;  // inode has been read from disk?

    // disk side information, only valid if this->valid == true
    short type; 
    short major;
    short minor;
    short num_link;
    uint size;
    uint addrs[NDIRECT + 1];
};

void inode_table_init();

struct inode *iget(uint dev, uint inum);
void iput(struct inode *ip);

struct inode * idup(struct inode *ip);

int readi(struct inode *ip, int user_dst, void *dst, uint off, uint n);
int writei(struct inode *ip, int user_src, void *src, uint off, uint n);

void ilock(struct inode *ip);
void iunlock(struct inode *ip);

struct inode *inode_by_name(char *path);
struct inode *inode_parent_by_name(char *path, char *name);

void itrunc(struct inode *ip);

*/
#endif // FS_INODE_H
