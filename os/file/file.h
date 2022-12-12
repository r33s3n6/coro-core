#ifndef __FILE_FILE_H__
#define __FILE_FILE_H__


#include <ccore/types.h>
#include <atomic/lock.h>

#include <fs/inode.h>

#define PIPESIZE 512

struct pipe {
    char data[PIPESIZE];
    uint nread;    // number of bytes read
    uint nwrite;   // number of bytes written
    int readopen;  // read fd is still open
    int writeopen; // write fd is still open
    spinlock lock;
};


// struct inode *create(char *path, short type, short major, short minor);
// void fileinit();
// void device_init();
// void fileclose(struct file *f);
// ssize_t fileread(struct file *f, void *dst_va, size_t len);
// ssize_t filewrite(struct file *f, void *src_va, size_t len);
// struct file *filealloc();
// int fileopen(char *path, int flags);
// struct file *filedup(struct file *f);
// int filestat(struct file *f, uint64 addr);




#endif //!__FILE_H__