#ifndef SYSCALL_SYSCALL_IMPL_H
#define SYSCALL_SYSCALL_IMPL_H

#include <ccore/types.h>

struct file_stat;

int sys_execv(char *pathname_va, char * argv_va[]);

int sys_exit(int status);

ssize_t sys_read(int fd, void *dst_va, size_t len);

ssize_t sys_write(int fd, void *src_va, size_t len);

pid_t sys_getpid(void);

pid_t sys_getppid();

int sys_open( char *pathname_va, int flags);

int sys_mknod( char *pathname_va, short major, short minor);

int sys_dup(int oldfd);

int sys_sched_yield(void);

pid_t sys_waitpid(pid_t pid, int *wstatus_va);

int sys_mkdir(char *pathname_va);

int sys_close(int fd);

pid_t sys_fork(void);

uint64 sys_time_ms();

int sys_sleep(unsigned long long time_in_ms);

int sys_pipe(int (*pipefd_va)[2]);

int sys_fstat(int fd, file_stat *statbuf_va);

int sys_chdir(char *path_va);

int sys_link( char *oldpath_va,  char *newpath_va);

int sys_unlink( char *pathname_va);

int64 sys_setpriority(int64 priority);

int64 sys_getpriority();

#endif // SYSCALL_IMPL_H
