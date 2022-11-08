#ifndef DEVICE_BLOCK_BUF_H
#define DEVICE_BLOCK_BUF_H

#include <ccore/types.h>
#include <atomic/lock.h>

#include <utils/wait_queue.h>
#include <utils/list.h>
#include <utils/utility.h>

#include <device/device.h>

#include <coroutine.h>
#include <optional>

#include "bdev.h"

struct block_buffer_node : noncopyable {
    bool valid = false; // has data been read from disk?

    bool in_use = false;
    bool dirty = false;
    // bool destroyed = false; // device unmounted
    bool dying = false;
    block_device * bdev;
    uint64 block_no;
    uint64 reference_count = 0;
    uint8 *data;
    wait_queue queue;
    spinlock lock {"block_buffer_node.lock"};
    // block_buffer_node(block_device* bdev, uint64 block_no) : bdev(bdev), block_no(block_no) {}
    block_buffer_node() {
    }

    ~block_buffer_node() {
        if (data) {
            delete[] data;
        }
        
    }
    private:
    // you should call this while in use
    task<void> __sync_to_device() {
        if(valid && dirty) {
            co_await bdev->write(block_no, 1, data);
            dirty = false;
        }
        co_return task_ok;
    }
    // you should call this while in use
    task<void> __sync_from_device() {
        
        if(!valid) {
            co_await bdev->read(block_no, 1, data);
            valid = true;
        }
        co_return task_ok;
    }

    void inc_ref() {
        lock.lock();
        reference_count++;
        lock.unlock();
    }
    void dec_ref() {
        lock.lock();
        if(reference_count == 0) {
            panic("block_buffer_node::dec_ref: reference_count == 0");
        }
        reference_count--;
        lock.unlock();
    }

    
    friend class block_buffer_node_ref;
    task<void> get();

    void put();

    public:

    bool match(block_device* bdev, uint64 block_no) {
        auto guard = make_lock_guard(lock);
        return this->bdev == bdev && this->block_no == block_no && !dying;
    }

    void init(block_device* bdev, uint64 block_no);

    void mark_dying(){
        lock.lock();
        dying = true;
        lock.unlock();
    }
    

    // flush when you not hold this buffer, and don't allow more reference to this buffer
    task<void> flush();
};

struct block_buffer_node_ref : noncopyable{
    public:
    block_buffer_node *node = nullptr;
    bool hold = false;
    block_buffer_node_ref() = default;
    block_buffer_node_ref(block_buffer_node *node) : node(node) {
        if(node) {
            node->inc_ref();
        }
    }
    ~block_buffer_node_ref() {
        if(node) {
            if(hold) {
                put();
            }
            node->dec_ref();
        }
    }


    block_buffer_node_ref& operator=(block_buffer_node_ref&& other) {
        std::swap(node, other.node);
        std::swap(hold, other.hold);
        return *this;
    }

    block_buffer_node_ref(block_buffer_node_ref&& other) {
        std::swap(node, other.node);
        std::swap(hold, other.hold);
    }


    explicit operator bool() const {
        return node != nullptr;
    }


    task<const uint8*> get_for_read() {
        co_return *co_await get();
    }

    task<uint8*> get_for_write() {
        uint8* ret = *co_await get();
        node->dirty = true;
        co_return ret;
    }

    void put() {
        if(!hold) {
            return;
        }
        hold = false;
        __sync_synchronize();
        node->put();
    }

    task<void> flush() {
        if (!hold) {
            co_await node->get();
        }

        co_await node->__sync_to_device();
        
        if (!hold) {
            node->put();
        }

        co_return task_ok;
    }

    private:
    task<uint8*> get();






};

// LRU cache of disk block contents.
// we can use radix tree to speed up search
class block_buffer {
    constexpr static int32 MIN_BUFFER_COUNT = 2048;
public:
    list<block_buffer_node> buffer_list;
    spinlock lock {"block_buffer.lock"};

    block_buffer() {}
    task<block_buffer_node_ref> get_node(device_id_t device_id, uint64 block_no);
    task<void> destroy(device_id_t device_id);

};

extern block_buffer kernel_block_buffer;
#endif // BUF_H
