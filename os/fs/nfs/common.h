#ifndef NFS_COMMON_H
#define NFS_COMMON_H

#include <ccore/types.h>


// On-disk file system format.
// Both the kernel and user programs use this header file.
namespace nfs {

// Disk layout:
// [ boot block | super block | free bit map ... | generic blocks ]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:


constexpr static uint32 NFS_MAGIC           = 0x10203040;
constexpr static uint32 BLOCK_SIZE          = 1024;
constexpr static uint32 INODE_TABLE_NUMBER  = 0;
constexpr static uint32 ROOT_INODE_NUMBER   = 1;
constexpr static uint32 SUPER_BLOCK_INDEX   = 1;
constexpr static uint32 BITMAP_BLOCK_INDEX  = 2;
constexpr static uint32 BITMAP_BLOCK_SIZE   = BLOCK_SIZE * 8;

constexpr static uint32 MAX_NAME_LENGTH     = 28;

constexpr static uint32 NUM_DIRECT_DATA     = 13;
constexpr static uint32 NUM_ADDRS           = (BLOCK_SIZE / sizeof(uint32)) - 1;

constexpr static uint32 ADDR_BLOCK_DATA_SIZE = NUM_ADDRS * BLOCK_SIZE;
constexpr static uint32 DIRECT_DATA_SIZE    = NUM_DIRECT_DATA * BLOCK_SIZE;

constexpr static uint32 MIN_CAPACITY        = 1024; // blocks

// On-disk inode structure
struct dinode {
    uint32 size;
    uint16 nlinks;
    uint8 perm;
    uint8 type;
    uint32 next_addr_block;
    uint32 addrs[NUM_DIRECT_DATA]; // data block addresses
};

struct superblock {
    uint32 magic;                   // NFS_MAGIC
    uint32 total_blocks;        
    uint32 total_generic_blocks;  
    uint32 used_generic_blocks; 
    uint32 ninodes;                 // number of used inodes.
    uint32 generic_block_start;     // block number of first generic block
    uint32 next_free_bitmap;        // next free bitmap block
    dinode inode_table;             // inode table
};

constexpr static uint32 INODES_PER_BLOCK    = BLOCK_SIZE / sizeof(dinode);

struct addr_block {
    uint32 next_addr_block;
    uint32 addrs[NUM_ADDRS];
};

struct dirent {
    uint32 inode_number;
    char name[MAX_NAME_LENGTH];
};


}; // namespace nfs



#endif
