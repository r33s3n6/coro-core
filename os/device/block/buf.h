#ifndef DEVICE_BLOCK_BUF_H
#define DEVICE_BLOCK_BUF_H

#include <ccore/types.h>

#include <coroutine.h>

#include <utils/utility.h>

#include <utils/buffer.h>

#include <device/device.h>
#include "bdev.h"

#include <atomic/lock.h>
#include <utils/list.h>
#include <utils/shared_ptr.h>


#include <optional>

#include <utils/buffer_manager.h>

class block_buffer_node : public referenceable_buffer<block_buffer_node> {
    public: // TODO: private
    block_device* bdev;
    uint64 block_no;

    public:
    uint8 *data;

    block_buffer_node() {
        data = new uint8[block_device::BLOCK_SIZE];
    }

    ~block_buffer_node() {
        if (data) {
            delete[] data;
        }
        
    }

    bool match(block_device* bdev, uint64 block_no) {
        return this->bdev == bdev && this->block_no == block_no;
    }

    bool match(device_id_t device_id) {
        return this->bdev && this->bdev->device_id == device_id;
    }

    uint32 get_block_no() const {
        return block_no;
    }

    void init(block_device* bdev = nullptr, uint64 block_no = 0) {
        this->bdev = bdev;
        this->block_no = block_no;
        this->mark_invalid();
    }

    void print();
    
    public:

    task<void> __load();
    task<void> __flush();

};

;



// LRU cache of disk block contents.
// we can use radix tree to speed up search
// class block_buffer {
//     constexpr static int32 MIN_BUFFER_COUNT = 2048;
//     constexpr static int32 MAX_BUFFER_COUNT = 10240;
//     
// public:
// 
//     using buffer_t = block_buffer_node;
//     using buffer_ref_t = reference_guard<buffer_t>;
//     using buffer_ptr_t = shared_ptr<buffer_t>;
// 
//     list<buffer_ptr_t> buffer_list;
//     list<buffer_ptr_t> flush_list;
//     wait_queue flush_queue;
// 
//     spinlock lock {"block_buffer.lock"};
// 
//     block_buffer() {}
//     task<buffer_ptr_t> get(bdev_t device_id, uint64 block_no);
//     task<void> destroy(device_id_t device_id);
// 
// };

using block_buffer_t = buffer_manager<block_buffer_node>;

extern block_buffer_t kernel_block_buffer;
#endif // BUF_H
