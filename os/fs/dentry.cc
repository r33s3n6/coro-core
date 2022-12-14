#include "dentry.h"
#include "inode.h"

#include <utils/log.h>


dentry_cache kernel_dentry_cache;


void dentry::set_inode(shared_ptr<inode> inode) {
    lock.lock();
    _inode = inode;
    lock.unlock();

}

dentry::~dentry() {

}

void dentry::print(){
    debugf("dentry: '%s', parent: %p, inode_ptr: %p\n", name.data(), parent, _inode ? _inode.get() : nullptr);
}

task<shared_ptr<dentry>> dentry_cache::__add(const quick_string_ref& name) {
    uint32 hash = name.hash();
    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    co_await entry.lock.lock();
    shared_ptr<dentry> new_dentry = make_shared<dentry>();
    entry.dentry_list.push_front(new_dentry);
    new_dentry->name = std::move(name.to_string());
    new_dentry->lock.lock();
    entry.lock.unlock();
    size++;
    co_return new_dentry;
}

task<shared_ptr<dentry>> dentry_cache::__reuse(const quick_string_ref& name) {
    // TODO: reuse dentry
    (void)(name);
    co_return nullptr;
}

task<shared_ptr<dentry>> dentry_cache::__get_free_dentry(const quick_string_ref& name_ref, shared_ptr<dentry> parent) {
    shared_ptr<dentry> new_dentry = nullptr;

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
    new_dentry->lock.unlock();
        
    co_return new_dentry;
}


task<shared_ptr<dentry>> dentry_cache::lookup(shared_ptr<dentry> parent, const quick_string_ref& name_ref) {
    uint32 hash = name_ref.hash();

    dentry_cache_entry& entry = hash_table[hash % HASH_TABLE_SIZE];
    co_await entry.lock.lock();
    for (auto it = entry.dentry_list.begin(); it != entry.dentry_list.end(); ++it) {
        auto& d = *it;
        d->lock.lock();
        if ((strncmp(d->name.data(), name_ref.data(),name_ref.size()) == 0) && (d->parent == parent)) {
            entry.dentry_list.move_to_front(it);
            d->lock.unlock();
            entry.lock.unlock();
            co_return d;
        }
        d->lock.unlock();
    }
    entry.lock.unlock();

    co_return nullptr;
}

task<shared_ptr<dentry>> dentry_cache::get(shared_ptr<dentry> parent, const quick_string_ref& name_ref) {
    shared_ptr<dentry> d = *co_await lookup(parent, name_ref);
    if (d) {
        co_return d;
    }

    shared_ptr<dentry> new_dentry = *co_await __get_free_dentry(name_ref, parent);


    // fill new dentry
    auto ret = *co_await parent->get_inode()->lookup(new_dentry);

    if (ret == -1) {
        new_dentry->lock.lock();
        new_dentry->parent = nullptr;
        new_dentry->lock.unlock();
        co_return nullptr;
    }

    co_return new_dentry;
}


task<shared_ptr<dentry>> dentry_cache::get(shared_ptr<dentry> parent, const char* name) {
    quick_string_ref name_ref(name);
    
    co_return *co_await get(parent, name_ref);
}

task<shared_ptr<dentry>> dentry_cache::get_or_create(shared_ptr<dentry> parent, const char* name, shared_ptr<inode> inode) {
    quick_string_ref name_ref(name);

    shared_ptr<dentry> d = *co_await lookup(parent, name_ref);
    if (d) {
        co_return d;
    }

    shared_ptr<dentry> new_dentry = *co_await __get_free_dentry(name_ref, parent);

    new_dentry->parent = parent;

    if (inode) {
        inode->set_dentry(new_dentry);
        new_dentry->set_inode(inode);
    }
    
    co_return new_dentry;
}

task<shared_ptr<dentry>> dentry_cache::create(shared_ptr<dentry> parent, const char* name, shared_ptr<inode> inode) {
    quick_string_ref name_ref(name);

    auto new_dentry = *co_await __get_free_dentry(name_ref, parent);

    new_dentry->parent = parent;

    if (inode) {
        inode->set_dentry(new_dentry);
        new_dentry->set_inode(inode);
    }

    co_return new_dentry;

}

task<shared_ptr<dentry>> dentry_cache::create(shared_ptr<dentry> parent, std::string_view name, shared_ptr<inode> inode) {
    quick_string_ref name_ref(name.data(), name.size());

    auto new_dentry = *co_await __get_free_dentry(name_ref, parent);

    new_dentry->parent = parent;

    if (inode) {
        inode->set_dentry(new_dentry);
        new_dentry->set_inode(inode);
    }

    co_return new_dentry;

}

task<shared_ptr<dentry>> dentry_cache::get_at(shared_ptr<dentry> current, const char* path) {

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
            if (*path == '/') {
                path++;
            }
            // empty path component
            continue;
        }

        if (path - start == 1 && *start == '.') {
            // current directory
            continue;
        }

        if (path - start == 2 && *start == '.' && *(start+1) == '.') {
            // parent directory
            if (current->parent == nullptr) {
                // already at root
                continue;
            }
            debugf("current: '%s' %p, parent: '%s' %p", current->name.data(), current, current->parent->name.data(), current->parent);
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

task<shared_ptr<dentry>> dentry_cache::get_at(shared_ptr<dentry> current, std::string_view path) {


    if (path.starts_with('/')) {
        current = *co_await get(nullptr, ""); // get root
        path.remove_prefix(1);
    }

    while (path.size() > 0) {

        std::string_view path_component;
        auto pos = path.find_first_of('/');

        if (pos == 0) { // empty path component
            path.remove_prefix(1);
            continue;
        } else if (pos == std::string_view::npos) {
            path_component = path;
            path.remove_prefix(path.size());
        } else {
            path_component = path.substr(0, pos);
            path.remove_prefix(pos + 1);
        }

        if (path_component.size() == 1 && path_component[0] == '.') {
            // current directory
            continue;
        }

        if (path_component.size() == 2 && path_component[0] == '.' && path_component[1] == '.') {
            // parent directory
            if (current->parent == nullptr) {
                // already at root
                continue;
            }
            debugf("current: '%s' %p, parent: '%s' %p", current->name.data(), current, current->parent->name.data(), current->parent);
            current = current->parent;
            continue;
        }


        quick_string_ref name_ref(path_component.data(), path_component.size());
        current = *co_await get(current, name_ref);

    }
    
    co_return current;
}



task<void> dentry_cache::destroy() {
    for(uint32 i = 0; i < HASH_TABLE_SIZE; i++) {
        dentry_cache_entry& entry = hash_table[i];
        co_await entry.lock.lock();
        entry.dentry_list.clear();
        entry.lock.unlock();
    }

    size = 0;

    co_return task_ok;
}

void dentry_cache::print() {
    for(uint32 i = 0; i < HASH_TABLE_SIZE; i++) {
        dentry_cache_entry& entry = hash_table[i];
        for (auto it = entry.dentry_list.begin(); it != entry.dentry_list.end(); ++it) {
            auto& d = *it;
            if (d) {
                d->print();
            }
            
        }
    }
}