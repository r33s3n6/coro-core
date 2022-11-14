#ifndef FS_DENTRY_H
#define FS_DENTRY_H

#include <utils/list.h>
#include <utils/quick_string.h>
#include <utils/shared_ptr.h>

#include <ccore/types.h>

#include <atomic/lock.h>
#include <atomic/mutex.h>

#include <mm/utils.h>
#include <coroutine.h>


class inode;



class dentry {
    private:
    inode* _inode = nullptr;
    public:
    quick_string name;


    dentry* parent = nullptr;
    
    uint32 reference_count = 0;
    spinlock lock {"dentry.lock"};
    
    list<dentry*> children;

    inode* get_inode() {
        return _inode;
    }
    
    ~dentry();

private:
    friend class inode;
    friend class dentry_cache;

    task<void> set_inode(inode* inode);




};

// dentry cache and path walk helper functions

struct dentry_cache_entry {
    list<dentry> dentry_list;
    coro_mutex lock {"dentry_cache_entry.lock"};
};

// dentry cache (hash table with lrucache)
class dentry_cache {
    constexpr static uint32 HASH_TABLE_SIZE = 1024;
    constexpr static uint32 MIN_CACHE_SIZE = 1024;

    dentry_cache_entry hash_table[HASH_TABLE_SIZE];

    public:


    void put(dentry* dentry);
    task<dentry*> get(dentry* parent, const quick_string_ref& name_ref);
    task<dentry*> get(dentry* parent, const char* name);
    task<dentry*> get_at(dentry* current, const char* path);
    task<dentry*> create(dentry* parent, const char* name, inode* inode);
    task<void> destroy();
    
    private:
    // return new dentry with lock held
    task<dentry*> __add(const quick_string_ref& name);

    task<dentry*> __reuse(const quick_string_ref& name);

    task<dentry*> __get_free_dentry(const quick_string_ref& name_ref, dentry* parent);


    uint32 size = 0;
    
    
};

extern dentry_cache kernel_dentry_cache;
#endif