#ifndef DEVICE_BLOCK_BUF_H
#define DEVICE_BLOCK_BUF_H

#include <ccore/types.h>
#include <atomic/lock.h>

#include <utils/wait_queue.h>
#include <utils/list.h>
#include <device/device.h>

#include <coroutine.h>
#include <optional>

#include "bdev.h"

struct block_buffer_node {
    bool valid = false; // has data been read from disk?
    // int disk_is_reading;  // does disk "own" buf?
    bool in_use = false;
    bool dirty = false;
    // device_id_t device_id;
    block_device * bdev;
    uint64 block_no;
    uint64 reference_count = 0;
    uint8 data[block_device::BLOCK_SIZE];
    wait_queue queue;
    spinlock lock {"block_buffer_node.lock"};
    block_buffer_node(block_device* bdev, uint64 block_no) : bdev(bdev), block_no(block_no) {}
    block_buffer_node() = default;



    
    task<void> write_to_device() {
        if(!bdev){
            co_return task_fail;
        }
        if(dirty) {
            co_await bdev->write(block_no, 1, data);
            dirty = false;
        }
        co_return task_ok;
    }

    task<void> read_from_device() {
        if(!bdev){
            co_return task_fail;
        }
        if(!valid) {
            co_await bdev->read(block_no, 1, data);
            valid = true;
        }
        co_return task_ok;
    }
    private:
    
    friend class block_buffer_node_ref;
    task<uint8*> __get() {
        lock.lock();
        while (in_use) {
            co_await queue.done(lock);
        } 

        in_use = true;
        //hold = true;
        lock.unlock();
        
        // only who hold buffer can access these fields, so no need to lock
        co_await read_from_device();

        co_return data;
    }

    void __put(){
        lock.lock();

        in_use = false;
        queue.wake_up_one();
        lock.unlock();
    }
};

struct block_buffer_node_ref {
    block_buffer_node *node = nullptr;
    bool hold = false;
    block_buffer_node_ref() = default;
    block_buffer_node_ref(block_buffer_node *node) : node(node) {
        if(node) {
            node->lock.lock();
            node->reference_count++;
            node->lock.unlock();
        }
    }
    ~block_buffer_node_ref() {
        if(node) {
            if(hold) {
                put();
            }
            node->lock.lock();
            node->reference_count--;
            node->lock.unlock();
        }
    }
    explicit operator bool() const {
        return node != nullptr;
    }


    task<const uint8*> get_for_read() {
        std::optional<uint8*> ret = co_await __get();
        co_return *ret;
    }

    task<uint8*> get_for_write() {
        std::optional<uint8*> ret = co_await __get();
        node->dirty = true;
        co_return *ret;
    }

    void put() {
        if(!hold) {
            return;
        }
        hold = false;
        node->__put();
    }

    task<void> flush() {
        co_await node->write_to_device();

        co_return task_ok;
    }

    private:
    task<uint8*> __get(){
        std::optional<uint8*> ret = co_await node->__get();
        hold = true;
        co_return *ret;
    }

};

// LRU cache of disk block contents.
class block_buffer {
public:
    list<block_buffer_node> buffer_list;
    spinlock lock {"block_buffer.lock"};

    block_buffer() {}
    task<block_buffer_node_ref> get_node(device_id_t device_id, uint64 block_no) {
        auto bdev = device::get<block_device>(device_id);
        lock.lock();
        auto unused = buffer_list.end();
        for(auto it = buffer_list.begin(); it != buffer_list.end(); ++it) {
            auto& node = *it;
            if(node.bdev == bdev && node.block_no == block_no) {
                // put node at the front of the list
                buffer_list.move_to_front(it);
                lock.unlock();
                co_return block_buffer_node_ref{&node};
            } else if (!node.in_use) {
                unused = it;
            }
        }
        
        if(unused == buffer_list.end()) {
            buffer_list.push_front(block_buffer_node(bdev, block_no));
            lock.unlock();
            co_return block_buffer_node_ref{&buffer_list.front()};
        }
        lock.unlock();

        auto& node = *unused;

        if(node.valid && node.dirty) {
            // write back
            co_await node.write_to_device();
        }

        node.bdev = bdev;
        node.block_no = block_no;
        node.valid = false;
        co_return block_buffer_node_ref{&node};
    }

};

extern block_buffer kernel_block_buffer;
#endif // BUF_H
