#ifndef FS_DENTRY_H
#define FS_DENTRY_H

#include <utils/list.h>
#include <utils/quick_string.h>
#include <utils/shared_ptr.h>

#include <ccore/types.h>

#include <atomic/lock.h>

#include <mm/utils.h>
#include <coroutine.h>


class inode;



class dentry {
    public:
    quick_string name;

    inode* _inode = nullptr;
    dentry* parent = nullptr;
    
    uint32 reference_count = 0;
    spinlock lock {"dentry.lock"};
    
    list<dentry*> children;


};

// dentry cache and path walk helper functions

struct dentry_cache_entry {
    list<dentry> dentry_list;
    spinlock lock {"dentry_cache_entry.lock"};
};

// dentry cache (hash table with lrucache)
class dentry_cache {
    constexpr static uint32 HASH_TABLE_SIZE = 1024;
    constexpr static uint32 MIN_CACHE_SIZE = 1024;

    dentry_cache_entry hash_table[HASH_TABLE_SIZE];

    public:


    void put(dentry& dentry) ;

    task<dentry*> walk(dentry* parent, const char* name);
    
    private:
    // return new dentry with lock held
    dentry* __add(const quick_string_ref& name);

    task<dentry*> __reuse(const quick_string_ref& name);

    task<dentry*> __get_free_dentry(const quick_string_ref& name_ref, dentry* parent);


    uint32 size = 0;
    
    
};

extern dentry_cache kernel_dentry_cache;
#endif