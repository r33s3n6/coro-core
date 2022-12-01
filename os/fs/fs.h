#ifndef FS_FS_H
#define FS_FS_H

#include "inode.h"

class filesystem {
public:
    virtual ~filesystem() = default;
    virtual task<void> mount(device_id_t _device_id) = 0;
    virtual task<void> unmount() = 0;

    virtual task<shared_ptr<inode>> get_root() = 0;
    device* get_device() {
        return dev;
    }
    device_id_t get_device_id() const { return dev->device_id; }
    protected:
    // device_id_t device_id;
    device* dev = nullptr;
};


#endif