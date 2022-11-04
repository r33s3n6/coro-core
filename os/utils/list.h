// simple linked list (use heap allocator)
#ifndef UTILS_LIST_H
#define UTILS_LIST_H

#include <utility>

template <typename data_t>
class list {
   public:
    struct node {
        data_t data;
        node* next;
        node* prev;
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
    }

    void push_back(data_t&& data) {
        node* new_node = new node;
        new_node->data = std::move(data);

        new_node->next = tail;
        new_node->prev = tail->prev;
        tail->prev->next = new_node;
        tail->prev = new_node;

        _size++;
    }

    void push_back(const data_t& data) {
        node* new_node = new node;
        new_node->data = data;

        new_node->next = tail;
        new_node->prev = tail->prev;
        tail->prev->next = new_node;
        tail->prev = new_node;

        _size++;
    }

    void push_front(const data_t& data) {
        node* new_node = new node;
        new_node->data = data;
        new_node->next = head->next;
        new_node->prev = head;
        head->next->prev = new_node;
        head->next = new_node;

        _size++;
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
        for (auto it = begin(); it != end(); ++it) {
            delete it.ptr;
        }
        _size = 0;
        head->next = tail;
        tail->prev = head;
    }

    iterator begin() { return iterator(head->next); }

    iterator end() { return iterator(tail); }

    iterator rbegin() { return iterator(tail->prev); }

    iterator rend() { return iterator(head); }

    void erase(iterator it) { __remove(it.ptr); }

    void insert_after(iterator it, const data_t& data) {
        node* new_node = new node;
        new_node->data = data;
        new_node->next = it.ptr->next;
        new_node->prev = it.ptr;
        it.ptr->next->prev = new_node;
        it.ptr->next = new_node;

        _size++;
    }
    void insert_before(iterator it, const data_t& data) {
        node* new_node = new node;
        new_node->data = data;
        new_node->next = it.ptr;
        new_node->prev = it.ptr->prev;
        it.ptr->prev->next = new_node;
        it.ptr->prev = new_node;

        _size++;
    }
    void move_to_front(iterator it) {
        it.ptr->prev->next = it.ptr->next;
        it.ptr->next->prev = it.ptr->prev;
        it.ptr->next = head->next;
        it.ptr->prev = head;
        head->next->prev = it.ptr;
        head->next = it.ptr;
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
};

#endif