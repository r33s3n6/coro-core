#ifndef __FILE_FILE_H__
#define __FILE_FILE_H__


#include <ccore/types.h>
#include <atomic/lock.h>





#include <fs/fs.h>


// pipe.h
#define PIPESIZE 512
// in-memory copy of an inode
#define major(dev) ((dev) >> 16 & 0xFFFF)
#define minor(dev) ((dev)&0xFFFF)
#define mkdev(m, n) ((uint)((m) << 16 | (n)))


// map major device number to device functions.
struct device_rw_handler {
    int64 (*read)(char *dst, int64 len, int to_user);
    int64 (*write)(char *src, int64 len, int from_user);
};

extern struct device_rw_handler device_rw_handler[];

struct pipe {
    char data[PIPESIZE];
    uint nread;    // number of bytes read
    uint nwrite;   // number of bytes written
    int readopen;  // read fd is still open
    int writeopen; // write fd is still open
    struct spinlock lock;
};

// file.h
struct file {
    enum {
        FD_NONE = 0,
        FD_PIPE,
        FD_INODE,
        FD_DEVICE
    } type;

    int ref; // reference count
    char readable;
    char writable;
    struct pipe *pipe; // FD_PIPE
    struct inode *ip;  // FD_INODE
    uint off;          // FD_INODE
    short major;       // FD_DEVICE
};



// int init_mailbox(struct mailbox *mb);
struct inode *create(char *path, short type, short major, short minor);
void fileinit();
void device_init();
void fileclose(struct file *f);
ssize_t fileread(struct file *f, void *dst_va, size_t len);
ssize_t filewrite(struct file *f, void *src_va, size_t len);
struct file *filealloc();
int fileopen(char *path, int flags);
struct file *filedup(struct file *f);
int filestat(struct file *f, uint64 addr);
#define FILE_MAX (128 * 16)

#define CONSOLE 1
#define CPU_DEVICE 2
#define MEM_DEVICE 3
#define PROC_DEVICE 4

#endif //!__FILE_H__