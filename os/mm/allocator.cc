#include "allocator.h"
#include "log/printf.h"

#include <map>

// temp heap
char heap[4096];
int top = 4096;

struct mm_block {
    void* ptr = nullptr;
    int size = 0;
    bool valid = false;
};

mm_block blocks[64];
int block_top = 0;

void* operator new(std::size_t size) {
    
    top -= size;
    void* ptr = &heap[top];
    blocks[block_top].ptr = ptr;
    blocks[block_top].size = size;
    blocks[block_top].valid = true;
    block_top++;
    printf("new %ld, ptr: %p\n", size, ptr);
    return ptr;
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size);
}

void trace_delete(){
    printf("trace delete\n");
}


void operator delete(void* ptr) {
trace_delete();
    int size = 0;
    for (int i = 0; i < block_top; i++) {
        if (blocks[i].ptr == ptr && blocks[i].valid) {
            blocks[i].valid = false;
            size = blocks[i].size;
            break;
        }
    }
    if (size == 0) {
        printf("delete error, ptr: %p\n", ptr);
        return;
    }

    for(int i = 0; i < size; i++){
        ((char*)ptr)[i] = 0xcd;
    }

    printf("delete: %p\n", ptr);
    
}

void operator delete(void* ptr, std::size_t size) {
    operator delete(ptr);
}

void operator delete[](void* ptr) {
    operator delete(ptr);
}