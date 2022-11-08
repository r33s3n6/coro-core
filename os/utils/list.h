// simple linked list (use heap allocator)
#ifndef UTILS_LIST_H
#define UTILS_LIST_H

#include <utility>
#include <utils/panic.h>
#include <utils/utility.h>

template <typename data_t>
class list : noncopyable {
   public:
    struct node {
        data_t data;
        node* next;
        node* prev;
        // char dummy[128];
    };

    struct iterator {
        node* ptr;
        iterator(node* ptr) : ptr(ptr) {}
        iterator& operator++() {
            ptr = ptr->next;
            return *this;
        }
        iterator& operator--() {
            ptr = ptr->prev;
            return *this;
        }
        bool operator==(const iterator& rhs) { return ptr == rhs.ptr; }
        bool operator!=(const iterator& rhs) { return ptr != rhs.ptr; }
        data_t& operator*() { return ptr->data; }
    };

    struct reverse_iterator {
        node* ptr;
        reverse_iterator(node* ptr) : ptr(ptr) {}
        reverse_iterator& operator++() {
            ptr = ptr->prev;
            return *this;
        }
        reverse_iterator& operator--() {
            ptr = ptr->next;
            return *this;
        }
        bool operator==(const reverse_iterator& rhs) { return ptr == rhs.ptr; }
        bool operator!=(const reverse_iterator& rhs) { return ptr != rhs.ptr; }
        data_t& operator*() { return ptr->data; }
    };

    list() {
        head = new node;
        tail = new node;
        head->next = tail;
        tail->prev = head;
        _size = 0;
    }

    ~list() {
        clear();
        delete head;
        delete tail;
        head = nullptr;
        tail = nullptr;
    }


    data_t& push_back(){
        return __push_back()->data;
    }

    data_t& push_front(){
        return __push_front()->data;
    }

    template <typename X>
    void push_back(X&& data) {
        static_assert(std::is_same_v<data_t, std::decay_t<X>>, "X must be the same type of data_t");
        node* new_node = __push_back();
        new_node->data = std::forward<X>(data);
    }

    template <typename X>
    void push_front(X&& data) {
        static_assert(std::is_same_v<data_t, std::decay_t<X>>, "X must be the same type of data_t");
        node* new_node = __push_front();
        new_node->data = std::forward<X>(data);
    }

    data_t& front() { return head->next->data; }

    data_t& back() { return tail->prev->data; }

    void pop_front() {
        if (head->next != tail) {
            __remove(head->next);
        }
    }

    void pop_back() {
        if (tail->prev != head) {
            __remove(tail->prev);
        }
    }

    bool empty() { return head->next == tail; }

    void clear() {
        node* current= head->next;
        while(current != tail){
            node* next = current->next;
            delete current;
            current = next;
        }
        _size = 0;
        head->next = tail;
        tail->prev = head;
    }

    iterator begin() { return iterator(head->next); }

    iterator end() { return iterator(tail); }

    reverse_iterator rbegin() { return reverse_iterator(tail->prev); }

    reverse_iterator rend() { return reverse_iterator(head); }

    void merge(list& other, iterator other_it) {
        if (other_it == other.end()) {
            return;
        }
        // detach other_it
        node* other_it_prev = other_it.ptr->prev;
        node* other_it_next = other_it.ptr->next;

        other_it_prev->next = other_it_next;
        other_it_next->prev = other_it_prev;

        // attach other_it
        tail->prev->next = other_it.ptr;
        other_it.ptr->prev = tail->prev;

        tail->prev = other_it.ptr;
        other_it.ptr->next = tail;

        _size += 1;
        other._size -= 1;

    }

    void merge(list& other) {
        if (other.empty()) {
            return;
        }
        tail->prev->next = other.head->next;
        other.head->next->prev = tail->prev;

        tail->prev = other.tail->prev;
        other.tail->prev->next = tail;

        _size += other._size;

        other.head->next = other.tail;
        other.tail->prev = other.head;
        other._size = 0;
    }

    void erase(iterator it) { 
        if (it.ptr == head || it.ptr == tail) {
            panic("list::erase");
        }
        __remove(it.ptr);
    }

    void insert_after(iterator it, const data_t& data) {
        if (it.ptr == tail) {
            panic("insert_after: iterator is end()");
        }
        node* new_node = new node;
        new_node->data = data;
        new_node->next = it.ptr->next;
        new_node->prev = it.ptr;
        it.ptr->next->prev = new_node;
        it.ptr->next = new_node;

        _size++;
    }
    void insert_before(iterator it, const data_t& data) {
        if (it.ptr == head) {
            panic("insert_before: iterator is begin()");
        }
        node* new_node = new node;
        new_node->data = data;
        new_node->next = it.ptr;
        new_node->prev = it.ptr->prev;
        it.ptr->prev->next = new_node;
        it.ptr->prev = new_node;

        _size++;
    }


    void move_to_front(iterator it) {
        __move_to_front(it.ptr);
    }

    void move_to_front(reverse_iterator it) {
        __move_to_front(it.ptr);
    }

    int size() { return _size; }


   private:
    void __remove(node* node) {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        delete node;
        _size--;
    }
private: 
    node* head;
    node* tail;
    int _size;

    node* __push_back() {
        node* new_node = new node;

        new_node->next = tail;
        new_node->prev = tail->prev;
        tail->prev->next = new_node;
        tail->prev = new_node;

        _size++;
        return new_node;
    }

    node* __push_front() {
        node* new_node = new node;

        new_node->next = head->next;
        new_node->prev = head;
        head->next->prev = new_node;
        head->next = new_node;

        _size++;
        return new_node;
    }

    void __move_to_front(node* ptr) {
        if (ptr == head || ptr == tail) {
            panic("__move_to_front: iterator is begin() or end()");
        }
        if (ptr->prev == head) {
            return;
        }
        ptr->prev->next = ptr->next;
        ptr->next->prev = ptr->prev;
        ptr->next = head->next;
        ptr->prev = head;
        head->next->prev = ptr;
        head->next = ptr;
    }

};

#endif