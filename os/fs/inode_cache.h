#ifndef FS_INODE_CACHE_H
#define FS_INODE_CACHE_H

#include <utils/buffer_manager.h>
#include "inode.h"

using inode_cache_t = buffer_manager<inode>;

extern inode_cache_t kernel_inode_cache;

#endif