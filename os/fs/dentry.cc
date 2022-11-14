#include "dentry.h"
#include "inode.h"

#include <utils/log.h>


dentry_cache kernel_dentry_cache;


task<void> dentry::set_inode(inode* inode) {
    if (_inode) {
        // _inode->set_dentry(nullptr);
        co_await _inode->put();
    }

    lock.lock();
    _inode = inode;
    lock.unlock();

    if (inode) {
        inode->get();
    }

    
    co_return task_ok;
}

dentry::~dentry() {
    if(_inode != nullptr) {
        _inode->this_dentry = nullptr;
        _inode->put();
    }
}

task<dentry*> dentry_cache::__add(const quick_string_ref& name) {
    uint32 hash = name.hash();
    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    co_await entry.lock.lock();
    dentry& new_dentry = entry.dentry_list.push_front();
    new_dentry.name = std::move(name.to_string());
    new_dentry.lock.lock();
    entry.lock.unlock();
    size++;
    co_return &new_dentry;
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
        new_dentry = *co_await __add(name_ref);
    }

    new_dentry->parent = parent;
    new_dentry->reference_count = 1;
    new_dentry->lock.unlock();
        
    co_return new_dentry;
}

void dentry_cache::put(dentry* dentry) {
    dentry->lock.lock();
    dentry->reference_count--;
    dentry->lock.unlock();
}

task<dentry*> dentry_cache::get(dentry* parent, const quick_string_ref& name_ref) {
    uint32 hash = name_ref.hash();

    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    co_await entry.lock.lock();
    for (auto it = entry.dentry_list.begin(); it != entry.dentry_list.end(); ++it) {
        it->lock.lock();
        if ((strncmp(it->name.data(), name_ref.data(),name_ref.size()) == 0) && (it->parent == parent)) {
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
    co_await parent->get_inode()->lookup(new_dentry);

    co_return new_dentry;
}


task<dentry*> dentry_cache::get(dentry* parent, const char* name) {
    quick_string_ref name_ref(name);
    
    co_return *co_await get(parent, name_ref);
}

task<dentry*> dentry_cache::create(dentry* parent, const char* name, inode* inode) {
    quick_string_ref name_ref(name);

    auto new_dentry = *co_await __get_free_dentry(name_ref, parent);

    new_dentry->parent = parent;

    if (inode) {
        co_await inode->set_dentry(new_dentry);
    }

    co_return new_dentry;

}

task<dentry*> dentry_cache::get_at(dentry* current, const char* path) {

    const char* start = nullptr;
    if (*path == '/') {
        current = *co_await get(nullptr, ""); // get root
        path++;
    }

    while (*path != '\0') {
        start = path;
        while (*path != '/' && *path != '\0') {
            path++;
        }

        if (path == start) {
            // empty path component
            continue;
        }

        if (path-start == 1 && *start == '.') {
            // current directory
            continue;
        }

        if (path-start == 2 && *start == '.' && *(start+1) == '.') {
            // parent directory
            if (current->parent == nullptr) {
                // already at root
                continue;
            }
            current = current->parent;
            continue;
        }
        quick_string_ref name_ref(start, path - start);
        current = *co_await get(current, name_ref);
        if (*path == '/') {
            path++;
        }
    }
    
    co_return current;
}


task<void> dentry_cache::destroy() {
    for(uint32 i = 0; i < HASH_TABLE_SIZE; i++) {
        dentry_cache_entry& entry = hash_table[i];
        co_await entry.lock.lock();
        for (auto it = entry.dentry_list.begin(); it != entry.dentry_list.end(); ++it) {
            
            it->lock.lock();
            uint32 ref_count = it->reference_count;
            it->lock.unlock();

            co_await it->set_inode(nullptr);
            size--;

            if (ref_count != 0){
                warnf("dentry %s still has %d references", it->name.data(), ref_count);
            }
        }
        entry.lock.unlock();
    }

    co_return task_ok;
}