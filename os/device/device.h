#ifndef DEVICE_DEVICE_H
#define DEVICE_DEVICE_H
#include <ccore/types.h>
#include <utils/list.h>

#include <utils/printf.h>


struct device_id_t {
    uint32 major;
    uint32 minor;
    bool operator==(const device_id_t &other) const {
        return major == other.major && minor == other.minor;
    }
};

#define RAMDISK_MAJOR 1
#define RAMDISK_MINOR 0

constexpr device_id_t ramdisk_id{RAMDISK_MAJOR, RAMDISK_MINOR};


#define VIRTIO_DISK_MAJOR 2
#define VIRTIO_DISK_MINOR 0

constexpr device_id_t virtio_disk_id{VIRTIO_DISK_MAJOR, VIRTIO_DISK_MINOR};




class device {
public:
    device_id_t device_id;

    void register_device(device_id_t _id) {
        device_id = _id;
        devices.push_back(this);
    }

    void deregister_device() {
        for (auto it = devices.begin(); it != devices.end(); ++it) {
            if (*it == this) {
                devices.erase(it);
                return;
            }
        }
        device_id = {0, 0};
    }

    static list<device*> devices;

    template< typename device_t >
    static device_t* get(device_id_t device_id) {
        for (auto device : devices) {
            if (device->device_id == device_id) {
                return static_cast<device_t*>(device);
            }
        }
        printf("WARN: device not found: %d:%d", device_id.major, device_id.minor);
        return nullptr;
    }
};

#endif