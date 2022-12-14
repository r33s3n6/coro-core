#include "ramdisk.h"

ramdisk::ramdisk(uint64 _nblocks) : block_device("ramdisk"), nblocks(_nblocks) {
    page_index_index = (uint8 ***)kernel_allocator.alloc_page();
    if (!page_index_index) 
        return;
    memset(page_index_index, 0, PGSIZE);
    debugf("ramdisk::ramdisk: %p, nblocks = %d, page_index_index: %p", this, this->nblocks, page_index_index);
}

ramdisk::~ramdisk() {
    for (uint64 i=0; i<page_capacity; i++) {
        if (page_index_index[i] != nullptr) {
            for (uint64 j=0; j<page_capacity; j++) {
                if (page_index_index[i][j] != nullptr) {
                    kernel_allocator.free_page(page_index_index[i][j]);
                }
            }
            kernel_allocator.free_page(page_index_index[i]);
        }
    }
    kernel_allocator.free_page(page_index_index);
}

uint64 ramdisk::capacity() const {
    return nblocks;
}
int ramdisk::open(void*) {
    if (!page_index_index)
        return -1;
    register_device(ramdisk_id);
    debugf("ramdisk: opened");
    return 0;
}
void ramdisk::close() {

    deregister_device();
    debugf("ramdisk: closed");
}

task<uint8*> ramdisk::walk(uint64 block_no, uint64* offset) {
    if (block_no > nblocks) {
        warnf("ramdisk %p: read block %d is larger than nblocks %d", this, block_no, nblocks);
        co_return task_fail;
    }
    uint64 page_index = block_no * BLOCK_SIZE / PGSIZE;
    uint64 page_offset = block_no * BLOCK_SIZE % PGSIZE;

    
    uint64 first_index = page_index / page_capacity;
    uint64 second_index = page_index % page_capacity;

    uint8** page_index_ptr = page_index_index[first_index];

    if (page_index_ptr == nullptr) {
        page_index_ptr = (uint8 **)kernel_allocator.alloc_page();
        if (!page_index_ptr)
            co_return task_fail;
        page_index_index[first_index] = page_index_ptr;
        memset(page_index_ptr, 0, PGSIZE);
    }

    uint8* page_ptr = page_index_ptr[second_index];

    if (page_ptr == nullptr) {
        page_ptr = (uint8 *)kernel_allocator.alloc_page();
        if (!page_ptr)
            co_return task_fail;
        page_index_ptr[second_index] = page_ptr;
        memset(page_ptr, 0, PGSIZE);
    }

    *offset = page_offset;
    co_return page_ptr;

}

task<int> ramdisk::read(uint64 block_no, uint64 count, void *buf) {

    if (device_id != ramdisk_id) {
        warnf("ramdisk: read called on non-ramdisk device");
        co_return task_fail;
    }
    
    uint64 offset;
    const uint8* page_ptr = *co_await walk(block_no, &offset);
    memcpy(buf, (const void*)(page_ptr + offset), count * BLOCK_SIZE);

    //if (block_no <64) {
        //debugf("ramdisk read %d: first 8 byte: %p, %p <- %p", block_no, (void*)*(uint64*)(page_ptr + offset), (void*)buf, (void*)(page_ptr + offset));
    //}
   

    co_return 0; // TODO: replace with actual return value
    
}
task<int> ramdisk::write(uint64 block_no, uint64 count, const void *buf) {

    if (device_id != ramdisk_id) {
        warnf("ramdisk: write called on non-ramdisk device");
        co_return task_fail;
    }

    uint64 offset;
    uint8* page_ptr = *co_await walk(block_no, &offset);
    
    memcpy((void*)(page_ptr + offset), buf, count * BLOCK_SIZE);

    //if (block_no < 64) {
        //debugf("ramdisk write %d: first 8 byte: %p, %p -> %p", block_no, (void*)*(uint64*)(page_ptr + offset), (void*)buf, (void*)(page_ptr + offset));
    //}
    co_return 0; // TODO: replace with actual return value
    
}

task<int> ramdisk::flush() {
    __sync_synchronize();
    co_return 0;
}