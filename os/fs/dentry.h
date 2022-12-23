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

#include <string>

class inode;



class dentry {
    private:
    shared_ptr<inode> _inode = nullptr;
    public:
    quick_string name;


    shared_ptr<dentry> parent = nullptr;
    
    // uint32 reference_count = 0;
    spinlock lock {"dentry.lock"};
    
    shared_ptr<inode> get_inode() {
        return _inode;
    }
    
    ~dentry();

    void print();

private:
    friend class inode;
    friend class dentry_cache;

    public:

    void set_inode(shared_ptr<inode> inode);




};

// dentry cache and path walk helper functions

struct dentry_cache_entry {
    list<shared_ptr<dentry>> dentry_list;
    coro_mutex lock {"dentry_cache_entry.lock"};
};

// dentry cache (hash table with lrucache)
class dentry_cache {
    constexpr static uint32 HASH_TABLE_SIZE = 1024;
    constexpr static uint32 MIN_CACHE_SIZE = 1024;

    dentry_cache_entry hash_table[HASH_TABLE_SIZE];

    public:


    void put(shared_ptr<dentry> dentry);
    task<shared_ptr<dentry>> get(shared_ptr<dentry> parent, const quick_string_ref& name_ref);
    task<shared_ptr<dentry>> get(shared_ptr<dentry> parent, const char* name);

    task<shared_ptr<dentry>> get_at(shared_ptr<dentry> current, const char* path);
    task<shared_ptr<dentry>> get_at(shared_ptr<dentry> current, std::string_view path);
    task<shared_ptr<dentry>> get_or_create(shared_ptr<dentry> parent, const char* name, shared_ptr<inode> inode);
    task<shared_ptr<dentry>> create(shared_ptr<dentry> parent, std::string_view name, shared_ptr<inode> inode);
    task<shared_ptr<dentry>> create(shared_ptr<dentry> parent, const char* name, shared_ptr<inode> inode);
    task<shared_ptr<dentry>> lookup(shared_ptr<dentry> parent, const quick_string_ref& name_ref);

    task<void> destroy();

    void print();
    
    private:
    // return new dentry with lock held
    task<shared_ptr<dentry>> __add(const quick_string_ref& name);

    task<shared_ptr<dentry>> __reuse(const quick_string_ref& name);

    task<shared_ptr<dentry>> __get_free_dentry(const quick_string_ref& name_ref, shared_ptr<dentry> parent);

    public:
    uint32 size = 0;

    
    
    
};

extern dentry_cache kernel_dentry_cache;
#endif