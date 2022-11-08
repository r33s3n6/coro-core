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

enum inode_type : uint8 {
    T_DIR     = 1,   // Directory
    T_FILE    = 2,   // File
};

// On-disk inode structure
struct dinode {
    uint32 size;
    uint16 nlinks;
    uint8 perm;
    uint8 type;
    uint32 next_addr_block;
    uint32 addrs[NUM_DIRECT_DATA]; // Data block addresses
};

struct superblock {
    uint32 magic;               // Must be FSMAGIC
    uint32 total_blocks;        // Size of file system image (blocks)
    uint32 total_generic_blocks;  // Number of data blocks
    uint32 used_generic_blocks; // Number of used data blocks
    
    uint32 ninodes;              // Number of used inodes.

    uint32 generic_block_start; // Block number of first generic block
    uint32 next_free_bitmap;    // Next free bitmap block
    dinode inode_table;         // Inodes
};



constexpr static uint32 INODES_PER_BLOCK    = BLOCK_SIZE / sizeof(dinode);

struct addr_block {
    uint32 next_addr_block;
    uint32 addrs[NUM_ADDRS];
};

struct dinode_block {
    struct dinode inodes[INODES_PER_BLOCK];
};

struct dirent {
    uint32 inode_number;
    char name[MAX_NAME_LENGTH];
};

constexpr static uint32 DIRENTS_PER_BLOCK = BLOCK_SIZE / sizeof(dirent);

struct dirent_block {
    dirent dirents[DIRENTS_PER_BLOCK];
};


}; // namespace nfs



#endif
