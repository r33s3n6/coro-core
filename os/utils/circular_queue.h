#ifndef UTILS_CIRCULAR_QUEUE_H
#define UTILS_CIRCULAR_QUEUE_H

#include <utils/utility.h>

template <typename data_t, int block_size = 512>
struct circular_queue_block {
    struct __links_t {
        circular_queue_block* next = nullptr;
        circular_queue_block* prev = nullptr;
    } links;
    static constexpr size_t capacity = (block_size-sizeof(__links_t))/sizeof(data_t);
    data_t data[capacity];
}; 

template <typename data_t, int block_size = 512>
class circular_queue : noncopyable {
    using block_t = circular_queue_block<data_t, block_size>;

    // fast access to head and tail
    block_t* head = nullptr;
    block_t* tail = nullptr;
    int head_index = 0;
    int tail_index = 0;

    int _size = 0;
    
    block_t* alloc_block() {
        return new block_t;
    }
    void free_block(block_t* block) {
        delete block;
    }
    
    circular_deque() {
        
    }

    ~circular_deque() {
        clear();
    }

    void clear() {
        block_t* current = head;
        do {
            block_t* next = current->links.next;
            free_block(current);
            current = next;
        } while (current != head);

        head = nullptr;
        tail = nullptr;
        head_index = 0;
        tail_index = 0;
        _size = 0;

    }

    void init_block(){
        head = alloc_block();
        tail = head;
        head->links.next = head;
        head->links.prev = head;
        head_index = 0;
        tail_index = 0;
        _size = 0;
    }

    void push(const data_t& data) {
        if (head == nullptr) {
            init_block();
        }
        tail->data[tail_index] = data;
        tail_index++;
        _size++;

        if (tail_index == block_t::capacity) {
            block_t* next_block = tail->links.next;
            if (next_block == head) { // alloc new block
                next_block = alloc_block();
                next_block->links.next = head;
                next_block->links.prev = tail;
                head->links.prev = next_block;
                tail->links.next = next_block;
            }
            tail = next_block;
            tail_index = 0;
        }
    }

    data_t& front() {
        return head->data[head_index];
    }
};

#endif