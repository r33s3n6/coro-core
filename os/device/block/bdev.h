#ifndef DEVICE_BLOCK_BDEV_H
#define DEVICE_BLOCK_BDEV_H


#include <ccore/types.h>
#include <coroutine.h>
#include <device/device.h>
#include <utils/wait_queue.h>

class block_device : public device {
    public:
    constexpr static uint32 BLOCK_SIZE = 1024;
public:
    block_device(const char *name) : device_name(name) {}
    virtual ~block_device() = default;
    // virtual uint64 size() const = 0;
    virtual uint64 capacity() const = 0;
    virtual int open(void* base_address) = 0;
    virtual void close() = 0;
    virtual task<int> read(uint64 block_no, uint64 count, void *buf) = 0;
    virtual task<int> write(uint64 block_no, uint64 count, const void *buf) = 0;
    virtual task<int> flush() = 0;

    protected:
    
    const char* device_name;

};

#endif