#include "buf.h"

#include <utils/log.h>
#include <utils/assert.h>

block_buffer kernel_block_buffer;

task<void> block_buffer_node::get() {
    lock.lock();
    while (in_use) {
        co_await queue.done(lock);
    } 
    
    in_use = true;
    lock.unlock();
    //debugf("block_buffer_node::get: block_no=%d", block_no);
    co_return task_ok;
}

void block_buffer_node::put(){
    lock.lock();

    in_use = false;
    queue.wake_up_one();
    lock.unlock();
}

task<void> block_buffer_node::flush(){

    // debugf("block_buffer_node::flush: in_use: %d, ref_count: %d, block_no: %d", in_use, reference_count, block_no);

    // uint64* data_buf = (uint64*)data;
    // debugf("first 64 bytes: %p %p %p %p %p %p %p %p", (void*)data_buf[0],  (void*)data_buf[1],  (void*)data_buf[2],
    //  (void*) data_buf[3],  (void*)data_buf[4],  (void*)data_buf[5],  (void*)data_buf[6],  (void*)data_buf[7]);

    co_await get();

    //debugf("block_buffer_node::flush: get the buffer done");

    co_await __sync_to_device();

    // debugf("block_buffer_node::flush: sync to device done");

    put();
    
    co_return task_ok;
}

task<uint8*> block_buffer_node_ref::get(){
    if (!hold) {
        co_await node->get();
        hold = true;
        // debugf("block_buffer_node_ref::get: block_no = %l", node->block_no);
        // if(node->block_no > node->bdev->capacity()) {
        //     panic("block_buffer_node::__sync_from_device: block_no > bdev->capacity()");
        // }
        
        co_await node->__sync_from_device();
        
    }

    // debugf("block_buffer_node_ref::get: block_no = %d", node->block_no);
    // if ( node->block_no == 2 || node->block_no == 20) {
    //     uint64* data_buf = (uint64*)node->data;
    //     debug_core("[block 2] first 8 bytes: %p", (void*)data_buf[0]);
    // }
    

    co_return node->data;
}

void block_buffer_node::init(block_device* bdev, uint64 block_no) {
    if (reference_count != 0) {
        panic("block_buffer_node.init: reference_count != 0");
    }

    if (!data) {
        data = new uint8[block_device::BLOCK_SIZE];
    }

    // debugf("block_buffer_node::init: %d -> %d",this->block_no, block_no);
    
    this->bdev = bdev;
    this->block_no = block_no;
    valid = false;
    dirty = false;
    dying = false;
    in_use = false;
    reference_count = 0;

}

task<block_buffer_node_ref> block_buffer::get_node(device_id_t device_id, uint64 block_no) {
    auto bdev = device::get<block_device>(device_id);
    lock.lock();
    

    for(auto it = buffer_list.begin(); it != buffer_list.end(); ++it) {
        auto& node = *it;
        if(node.match(bdev, block_no)) {
            // put node at the front of the list
            buffer_list.move_to_front(it);
            lock.unlock();
            co_return block_buffer_node_ref{&node};
        }
    }

    auto unused = buffer_list.rend();

    // find unused node first, backwards
    if (buffer_list.size() >= MIN_BUFFER_COUNT) {
        for (auto it = buffer_list.rbegin(); it != buffer_list.rend(); ++it) {
            auto& node = *it;

            auto guard = make_lock_guard(node.lock);
            if (node.reference_count == 0) { // we have lock, so it's safe
                unused = it;
                break;
            }
        }
    }
    
    if(unused == buffer_list.rend()) {
        block_buffer_node& new_buffer = buffer_list.push_front();
        lock.unlock();

        new_buffer.init(bdev, block_no);
        co_return block_buffer_node_ref{&new_buffer};
    }

    buffer_list.move_to_front(unused);

    auto& node = *unused;

    // do not allow any new reference to this node
    node.mark_dying();

    lock.unlock();

    co_await node.flush();

    // we can assert there's no reference to this buffer
    node.init(bdev, block_no);

    co_return block_buffer_node_ref{&node};
}

task<void> block_buffer::destroy(device_id_t device_id) {
    list<block_buffer_node> flush_list;
    lock.lock();
    auto it = buffer_list.begin();
    while(it != buffer_list.end()) {
        auto old_it = it;
        ++it;

        auto& node = *old_it;
        auto guard = make_lock_guard(node.lock);
        if (node.bdev) {
            if(node.bdev->device_id == device_id) {
                kernel_assert(node.reference_count == 0, "block_buffer::destroy: node.reference_count != 0");
                // detach and put it into flush_list
                // no need to mark dying, because no one can access it via buffer_list
                flush_list.merge(buffer_list, old_it);
            }
        }
        
    }
    lock.unlock();

    for(auto& node : flush_list) {
        co_await node.flush();
        node.init(nullptr, 0);
    }

    // add it back to buffer_list

    lock.lock();
    buffer_list.merge(flush_list);
    lock.unlock();


    co_return task_ok;
}