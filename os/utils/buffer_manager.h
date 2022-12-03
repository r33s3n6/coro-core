#ifndef UTILS_BUFFER_MANAGER_H
#define UTILS_BUFFER_MANAGER_H


#include "buffer.h"
#include <utils/shared_ptr.h>

#include <utils/log.h>

template <typename buffer_t, int32 min_buffer_count = 2048, int32 max_buffer_count = 10240>
class buffer_manager {
    
public:

    using buffer_ref_t = reference_guard<buffer_t>;
    using buffer_ptr_t = shared_ptr<buffer_t>;

    list<buffer_ptr_t> buffer_list;

    list<buffer_ptr_t> flush_list;
    wait_queue flush_queue;

    spinlock lock {"buffer_manager.lock"};

    buffer_manager() {}

    uint64 size() {
        return buffer_list.size();
    }

    // try to find a buffer node in buffer list, if not found, create a new one
    // or reuse a node not in use
    template <typename derived_t, typename... Args> // for virtual buffer_t
    task<buffer_ptr_t> get_derived(Args&&... match_args) {
        lock.lock();

        // if what we require is in flush list, wait for it finish
        bool in_flush = false;
        do {
            in_flush = false;
            for (auto& node : flush_list) {
                if (node->match(std::forward<Args>(match_args)...)) {

                    in_flush = true;
                    // debugf("block_buffer: wait flush %d", block_no);
                    co_await flush_queue.done(lock);
                    break;
                }
            }
        } while(in_flush);


        for(auto it = buffer_list.begin(); it != buffer_list.end(); ++it) {
            auto& node = *it;
            if(node->match(std::forward<Args>(match_args)...)) {
                // put node at the front of the list
                buffer_list.move_to_front(it);
                lock.unlock();
                co_return node;
            }
        }

        auto unused = buffer_list.rend();

        // find unused node first, backwards
        if (buffer_list.size() >= min_buffer_count) {
            for (auto it = buffer_list.rbegin(); it != buffer_list.rend(); ++it) {
                auto& node = *it;

                // we are the only shared_ptr, then we detach weak_ptr to node
                if (node.try_detach_weak()) {
                    unused = it;
                    break;
                }
            }
        }

        buffer_ptr_t buf;

        // we must push a new node
        if(unused == buffer_list.rend()) {
            if (buffer_list.size() >= max_buffer_count) {
                // we failed
                lock.unlock();
                co_return task_fail;
            }

            // create a new node
            buf = make_shared<derived_t>();

        } else {
            buf = *unused;
            buffer_list.erase(unused); // do not let it be fetched by other task
        }

        if(!buf->is_dirty()) {

            buf->init(std::forward<Args>(match_args)...);
            buffer_list.push_front(buf);
            lock.unlock();

            co_return buf;
        }

        auto flush_it = flush_list.push_back(buf);

        lock.unlock();

        co_await buf->flush();

        lock.lock();
        flush_list.erase(flush_it);

        flush_queue.wake_up_all();

        // TODO: performance
        // merge it back is dangerous, because someone may added it too
        // we find if it is in buffer_list, if not, we add it
        // else we just init it to null
        buffer_ptr_t ret_buf;
        for(auto& node: buffer_list) {
            if(node->match(std::forward<Args>(match_args)...)) {
                ret_buf = node;
                break;
            }
        }

        if(!ret_buf) {
            buf->init(std::forward<Args>(match_args)...);
            buffer_list.push_front(buf);
            ret_buf = std::move(buf);
        } else {
            buf->init(); // init to null
            buffer_list.push_back(buf);
        }


        lock.unlock();

        co_return ret_buf;
    }

    template <typename... Args>
    task<buffer_ptr_t> get(Args&&... match_args) {
        co_return *co_await get_derived<buffer_t>(std::forward<Args>(match_args)...);
    }

    // TODO: Note: destroy makes size not that correct
    template <typename... Args>
    task<uint32> destroy(Args&&... match_args) {

        list<buffer_ptr_t> flush_list;

        lock.lock();
        uint32 failed_count = 0;

        auto it = buffer_list.begin();
        while(it != buffer_list.end()) {
            auto old_it = it;
            ++it;

            auto& node = *old_it;
            if(node->match(std::forward<Args>(match_args)...)) {
                if (node.try_detach_weak()) {
                    // detach and put it into flush_list
                    flush_list.merge(buffer_list, old_it);
                } else {
                    // we failed
                    buffer_list.erase(old_it);
                    failed_count++;
                }
            }
        }
        lock.unlock();

        for(auto& node : flush_list) {
            
            // debugf("buffer_manager: flush %p", node.get());
            co_await node->flush();
            node->init(); // init to null
        }

        // add it back to buffer_list

        lock.lock();
        buffer_list.merge(flush_list);
        lock.unlock();

        co_return failed_count;

    }

};


#endif