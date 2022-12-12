#ifndef UTILS_FPRINT_H
#define UTILS_FPRINT_H

#include <coroutine.h>
#include <ccore/types.h>
#include <fs/inode.h>
#include <fs/file.h>

task<void> __fprint_int64(file* f, int64 xx, int64 base, int64 sign);

task<void> __fprint(file* f, int64 xx, char format);

task<void> __fprint(file* f, void* ptr, char format);

task<void> __fprint(file* f, const char* str, char format);


#endif