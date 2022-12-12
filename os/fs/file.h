#ifndef FS_FILE_H
#define FS_FILE_H

#include <fs/inode.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

struct file_stat {
    uint32 dev;   // File system's disk device
    uint32 ino;   // Inode number
    uint16 type;  // Type of file
    uint16 nlink; // Number of links to file
    size_t size;  // Size of file in bytes
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

    virtual task<int32> stat(file_stat* stat)  { co_return -EPERM; };

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
                co_await f->_inode->get();
                //co_await f->_inode->rw_lock.lock();
            }
            co_return task_ok;
        }

        void unlock() {
            if(!f->in_rw){
                f->_inode->put();
                // f->_inode->rw_lock.unlock();
            }
        }

    };

    __file_rw_lock_t __file_rw_lock{this};


    protected:
    shared_ptr<dentry> parent = nullptr; // parent dentry
    shared_ptr<inode> _inode = nullptr; // related inode
    uint64 offset = 0;

    private:
    bool in_rw = false;
};

class simple_file : public file {
    public:

    simple_file(shared_ptr<inode> _inode){
       this->_inode = _inode;
    }

    ~simple_file() {
        if (opened) {
            panic("simple_file is not closed");
        }
    }

    task<int32> open() override {
        // _inode->get();
        offset = 0;
        opened = true;
        co_return 0;
    }

    task<int32> close() override {
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

#endif