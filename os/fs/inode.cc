#include "inode.h"

#include "dentry.h"

#include <utils/log.h>

task<dentry*> inode::get_dentry() {
    if (this_dentry == nullptr) {
        warnf("inode::get_dentry: inode has no dentry");
        co_return nullptr;
    }
    co_return this_dentry;

}

task<void> inode::set_dentry(dentry* _dentry) {
    if(this_dentry) {
        co_await this_dentry->set_inode(nullptr);
    }
    this_dentry = _dentry;
    if (this_dentry) {
        co_await this_dentry->set_inode(this);
    }
}
    
