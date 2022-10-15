#ifndef FS_INODE_H
#define FS_INODE_H

#include <string>
#include <list>

#include <ccore/types.h>
#include <ccore/errno.h>

#include <coroutine.h>
#include <atomic/lock.h>

#include <utils/utility.h>

class inode;

class dentry {
    std::string name;
    inode* _inode;
    dentry* parent;
    std::list<dentry*> children;
};



// coroutine inode
class inode {

public:
    inode(){}
    virtual ~inode() {}

    // no copy to maintain the reference count
    // TODO: inode_ref
    inode(const inode&) = delete;

    
    // inode operations
    virtual task<int32> truncate(int64 size)  { co_return -EPERM; };

    
    virtual task<int32> create(dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> link(dentry* old_dentry, dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> symlink(dentry* old_dentry, dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> unlink(dentry* old_dentry )  { co_return -EPERM; };
    virtual task<int32> mkdir(dentry* new_dentry)  { co_return -EPERM; };
    virtual task<int32> rmdir(dentry* old_dentry)  { co_return -EPERM; };


    // sync from disk to memory
    virtual task<int32> load()  { co_return -EPERM; };



    public:
    spinlock rw_lock;

public:
    
    uint32 device_number;
    uint32 inode_number;
    int32 reference_count = 0;
    
    bool valid;


};

class file {

    public:
        // file operations
    virtual task<int32> open()  { co_return -EPERM; };
    virtual task<int32> close()  { co_return -EPERM; };
    virtual task<int64> read(void *buf, uint64 size)  { co_return -EPERM; };
    virtual task<int64> write(const void *buf, uint64 size)  { co_return -EPERM; };

    virtual task<int64> llseek(int64 offset, uint32 whence)  { co_return -EPERM; };
    
    virtual task<int32> flush()  { co_return -EPERM; };

    virtual task<int64> tell()  { co_return -EPERM; };


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

        void lock() {
            f->__file_rw_lock.lock();
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
        void lock() {
            if(!f->in_rw){
                f->_inode->rw_lock.lock();
            }
        }

        void unlock() {
            if(!f->in_rw){
                f->_inode->rw_lock.unlock();
            }
        }

    };

    __file_rw_lock_t __file_rw_lock{this};


    protected:
    dentry *_dentry;
    inode *_inode; // cached inode

    private:
    bool in_rw = false;
};


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
