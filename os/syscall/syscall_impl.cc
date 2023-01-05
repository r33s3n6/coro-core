#include "syscall_impl.h"
#include <arch/timer.h>
#include <fs/inode.h>
#include <fs/file.h>
#include <proc/process.h>
#include <utils/log.h>
#include <mm/vmem.h>

#define min(a, b) (a) < (b) ? (a) : (b);

int sys_fstat(int fd, file_stat* statbuf_va){
    user_process *p = cpu::__my_cpu()->get_user_process();

    // invalid fd
    if (fd < 0 || fd >= FD_MAX) {
        warnf("invalid fd %d", fd);
        return -1;
    }

    file *f = p->get_file(fd);

    // invalid fd
    if (f == NULL) {
        warnf("fd %d is not opened", fd);
        return -1;
    }

    file_stat stat_buf;

    // int ret = f->stat(&stat_buf);
    int ret = -1; // TODO: implement stat
    if (ret < 0) {
        return ret;
    }

    // copy stat_buf to user space
    if (copyout(p->get_pagetable(), (uint64)statbuf_va, (void *)&stat_buf, sizeof(stat_buf)) < 0) {
        return -1;
    }

    return 0;
}

int sys_exit(int code) {
    user_process *p = cpu::__my_cpu()->get_user_process();
    p->exit(code);
    
    return 0;
}

