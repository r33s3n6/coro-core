#include "buf.h"

#include <utils/log.h>
#include <utils/assert.h>

block_buffer kernel_block_buffer;


task<void> block_buffer_node::__load() {
    // debugf("block_buffer: load node %d", block_no);
    co_await bdev->read(block_no, 1, data);
    co_return task_ok;
}
task<void> block_buffer_node::__flush() {
    // debugf("block_buffer: flush node %d", block_no);
    co_await bdev->write(block_no, 1, data);
    co_return task_ok;
}


task<block_buffer::buffer_ptr_t> block_buffer::get_node(device_id_t device_id, uint64 block_no) {
    auto bdev = device::get<block_device>(device_id);
    lock.lock();
    
    bool in_flush = false;
    do {
        in_flush = false;
        for (auto& node : flush_list) {
            if (node->match(bdev, block_no)) {

                in_flush = true;
                // debugf("block_buffer: wait flush %d", block_no);
                co_await flush_queue.done(lock);
                break;
            }
        }
    } while(in_flush);


    for(auto it = buffer_list.begin(); it != buffer_list.end(); ++it) {
        auto& node = *it;
        if(node->__get_derived().match(bdev, block_no)) {
            // put node at the front of the list
            buffer_list.move_to_front(it);
            lock.unlock();
            co_return node;
        }
    }

    auto unused = buffer_list.rend();

    // find unused node first, backwards
    if (buffer_list.size() >= MIN_BUFFER_COUNT) {
        for (auto it = buffer_list.rbegin(); it != buffer_list.rend(); ++it) {
            auto& node = *it;

            // we are the only shared_ptr, then we detach weak_ptr to node
            if (node.try_detach_weak()) {
                unused = it;
                break;
            }
        }
    }

    // list<buffer_ptr_t> temp_list;
    buffer_ptr_t buf;


    // we must push a new node
    if(unused == buffer_list.rend()) {
        if (buffer_list.size() >= MAX_BUFFER_COUNT) {
            // we failed
            lock.unlock();
            co_return task_fail;
        }

        // create a new node
        buf = make_shared<block_buffer_node>();
        // debugf("block_buffer: create new node for %d", block_no);

    } else {
        buf = *unused;

        buffer_list.erase(unused);

    }

    if(!buf->is_dirty()) {

        buf->__get_derived().init(bdev, block_no);
        buffer_list.push_front(buf);
        lock.unlock();

        co_return buf;
    }

    auto flush_it = flush_list.push_back(buf);

    lock.unlock();

    // uint32 old_block_no = buf->__get_derived().get_block_no();

    // kernel_assert(buf.use_count()==1, "block_buffer::get_node: buf.use_count() != 1");
    // kernel_assert(buf->bdev!=nullptr, "block_buffer::get_node: buf->bdev == nullptr");
    
    co_await buf->flush();
    lock.lock();
    flush_list.erase(flush_it);

    flush_queue.wake_up_all();

    
    // debugf("block_buffer: block %d -> %d", old_block_no, block_no);

    // merge it back is dangerous, because someone may added it too
    
    // TODO: performance
    buffer_ptr_t ret_buf;
    for(auto& node: buffer_list) {
        if(node->__get_derived().match(bdev, block_no)) {
            ret_buf = node;
            break;
        }
    }

    buffer_list.push_front(buf);

    if(!ret_buf) {
        buf->__get_derived().init(bdev, block_no);
        ret_buf = std::move(buf);
    } else {
        buf->__get_derived().init(nullptr, 0);
    }
    
    
    lock.unlock();

    co_return ret_buf;
}

task<void> block_buffer::destroy(device_id_t device_id) {
    list<buffer_ptr_t> flush_list;
    lock.lock();

    uint32 failed_count = 0;

    auto it = buffer_list.begin();
    while(it != buffer_list.end()) {
        auto old_it = it;
        ++it;

        auto& node = *old_it;
        if(node->__get_derived().match(device_id)) {
            if (node.try_detach_weak()) {
                // detach and put it into flush_list
                flush_list.merge(buffer_list, old_it);
            } else {
                // we failed
                failed_count++;
                // warnf("block_buffer: block_no: %d, ref: %d", node->__get_derived().get_block_no(), node.use_count());
            }
        }
    }
    lock.unlock();

    for(auto& node : flush_list) {
        co_await node->flush();
        node->__get_derived().init(nullptr, 0);
    }

    // add it back to buffer_list

    lock.lock();
    buffer_list.merge(flush_list);
    lock.unlock();

    if (failed_count) {
        warnf("block_buffer: destroy failed: %d nodes are still in use", failed_count);
        co_return task_fail;
    }
    co_return task_ok;
}