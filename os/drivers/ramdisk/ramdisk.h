#ifndef DRIVERS_RAMDISK_RAMDISK_H
#define DRIVERS_RAMDISK_RAMDISK_H

#include <mm/allocator.h>

#include <device/block/bdev.h>


class ramdisk : public block_device {
    static constexpr uint64 page_capacity = PGSIZE/sizeof(void*);
    public:
    ramdisk(uint64 _nblocks);
    virtual ~ramdisk();
    virtual uint64 capacity() const override;
    virtual int open(void* base_address) override;
    virtual void close() override;
    virtual task<int> read(uint64 block_no, uint64 count, void *buf) override;
    virtual task<int> write(uint64 block_no, uint64 count, const void *buf) override;
    virtual task<int> flush() override;

    private:

    uint8*** page_index_index;
    uint64 nblocks; 

    task<uint8*> walk(uint64 block_no, uint64* offset);
};

#endif