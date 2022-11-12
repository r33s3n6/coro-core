#include "dentry.h"
#include "inode.h"

dentry_cache kernel_dentry_cache;

dentry* dentry_cache::__add(const quick_string_ref& name) {
    uint32 hash = name.hash();
    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    entry.lock.lock();
    dentry& new_dentry = entry.dentry_list.push_front();
    new_dentry.name = std::move(name.to_string());
    new_dentry.lock.lock();
    entry.lock.unlock();
    size++;
    return &new_dentry;
}

task<dentry*> dentry_cache::__reuse(const quick_string_ref& name) {
    // TODO: reuse dentry
    (void)(name);
    co_return nullptr;
}

task<dentry*> dentry_cache::__get_free_dentry(const quick_string_ref& name_ref, dentry* parent) {
    dentry* new_dentry = nullptr;

    // try to reuse
    if (size >= MIN_CACHE_SIZE) {
        auto task = __reuse(name_ref);
        task.get_promise()->no_yield = true;
        new_dentry = *co_await task;
    } 

    if(new_dentry == nullptr) {
        new_dentry = __add(name_ref);
    }

    new_dentry->parent = parent;
    new_dentry->reference_count = 1;
    new_dentry->lock.unlock();
        
    co_return new_dentry;
}

void dentry_cache::put(dentry& dentry) {
    dentry.lock.lock();
    dentry.reference_count--;
    dentry.lock.unlock();
}



task<dentry*> dentry_cache::walk(dentry* parent, const char* name) {
    quick_string_ref name_ref(name);
    uint32 hash = name_ref.hash();

    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    entry.lock.lock();
    for (auto it = entry.dentry_list.begin(); it != entry.dentry_list.end(); ++it) {
        it->lock.lock();
        if ((strcmp(it->name.data(), name) == 0) && (it->parent == parent)) {
            entry.dentry_list.move_to_front(it);
            it->reference_count++;
            it->lock.unlock();
            entry.lock.unlock();
            co_return &*it;
        }
        it->lock.unlock();
    }
    entry.lock.unlock();

    dentry* new_dentry = *co_await __get_free_dentry(name_ref, parent);

    // fill new dentry
    co_await parent->_inode->lookup(new_dentry);
    
    co_return new_dentry;
}