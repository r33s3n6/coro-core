#ifndef DRIVERS_CONSOLE_H
#define DRIVERS_CONSOLE_H

#include <fs/inode.h>
#include <sbi/sbi.h>

class sbi_console_file : public file {
    public:


    sbi_console_file(){
        _inode = make_shared<inode>();
    }

    ~sbi_console_file() {
        // delete _inode;
    }

    virtual task<int32> open() override {
        co_return 0;
    }

    virtual task<int32> close() override {
        co_return 0;
    }

    virtual task<int64> read(void *buf, uint64 size) override {
        char *cbuf = (char *)buf;

        
        co_await __file_rw_lock.lock();
        for (uint64 i = 0; i < size; i++) {
            cbuf[i] = sbi_console_getchar();
        }
        __file_rw_lock.unlock();

        co_return size;
    }

    virtual task<int64> write(const void *buf, uint64 size) override {
        const char *cbuf = (const char *)buf;


        co_await __file_rw_lock.lock();
        for (uint64 i = 0; i < size; i++) {
            sbi_console_putchar(cbuf[i]);
        }
        __file_rw_lock.unlock();


 

        co_return size;
    }


};

extern sbi_console_file sbi_console;

#endif

