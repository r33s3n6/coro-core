#include "inode.h"

#include "dentry.h"

#include <utils/log.h>

weak_ptr<dentry> inode::get_dentry() {
    if (this_dentry.expired()) {
        warnf("inode::get_dentry: inode has no dentry");
    }

    return this_dentry;

}

void inode::set_dentry(shared_ptr<dentry> _dentry) {
    this_dentry = _dentry.get_weak();
    // if (_dentry) {
    //     _dentry->set_inode(this);
    // }
}
    
