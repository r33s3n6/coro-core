#include "allocator.h"
#include "log/log.h"

#include <map>

// temp heap
char heap[4096];
std::size_t top = 4096;

struct mm_block {
    void* ptr = nullptr;
    std::size_t size = 0;
    bool valid = false;
};

mm_block blocks[64];
int block_top = 0;

// TODO: failed when no memory
void* operator new(std::size_t size) {

    void* ptr = nullptr;
    for(int i=0;i<block_top;i++){
        if(blocks[i].valid == false && blocks[i].size == size){
            blocks[i].valid = true;
            ptr = blocks[i].ptr;
            break;
        }
    }

    if(!ptr){
        
        if (top >= size){
            top -= size;
            ptr = &heap[top];
            blocks[block_top].ptr = ptr;
            blocks[block_top].size = size;
            blocks[block_top].valid = true;
            block_top++;
        }
    }
    
    
    if(!ptr){
        printf("new failed: %d\n",size);
    }
    
    // printf("new size %d, ptr: %p\n", size, ptr);
    return ptr;
}

void* operator new[](std::size_t size) {
    return operator new(size);
}

void* operator new (std::size_t size, const std::nothrow_t&) noexcept {
    return operator new(size);
}

void trace_delete(){
    // printf("trace delete\n");
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

    // printf("delete: %p\n", ptr);
    
}

void operator delete(void* ptr, std::size_t size) {
    operator delete(ptr);
}

void operator delete[](void* ptr) {
    operator delete(ptr);
}