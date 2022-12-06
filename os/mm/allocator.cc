#include <utils/printf.h>
#include <utils/panic.h>

#include "allocator.h"

#include <utils/log.h>
#include <map>

allocator kernel_allocator((void*)ekernel, (void*)IO_MEM_START);
allocator kernel_io_allocator((void*)(IO_MEM_START + 2*PGSIZE), (void*)PHYSTOP); // first 2 pages are reserved for virtio disk

heap_allocator<8> kernel_heap_allocator_8;
heap_allocator<16> kernel_heap_allocator_16;
heap_allocator<32> kernel_heap_allocator_32;
heap_allocator<64> kernel_heap_allocator_64;
heap_allocator<128> kernel_heap_allocator_128;
heap_allocator<256> kernel_heap_allocator_256;
heap_allocator<512> kernel_heap_allocator_512;
heap_allocator<1024> kernel_heap_allocator_1024;

const int tab64[64] = {
    63,  0, 58,  1, 59, 47, 53,  2,
    60, 39, 48, 27, 54, 33, 42,  3,
    61, 51, 37, 40, 49, 18, 28, 20,
    55, 30, 34, 11, 43, 14, 22,  4,
    62, 57, 46, 52, 38, 26, 32, 41,
    50, 36, 17, 19, 29, 10, 13, 21,
    56, 45, 25, 31, 35, 16,  9, 12,
    44, 24, 15,  8, 23,  7,  6,  5};

int log2_64 (uint64_t value)
{
    value |= value >> 1;
    value |= value >> 2;
    value |= value >> 4;
    value |= value >> 8;
    value |= value >> 16;
    value |= value >> 32;
    return tab64[((uint64_t)((value - (value >> 1))*0x07EDD5E59A4E28C2)) >> 58];
}


// TODO: failed when no memory
void* operator new(std::size_t size) {
    if(size > 4096) {
        warnf("operator new: size: %d", size);
        panic("operator new: size > 4096");
    }


    void* ptr = nullptr;

    if (size == 1) {
        ptr = kernel_heap_allocator_8.alloc();
    } else {
        switch (log2_64(size-1)) {
        case 0:
        case 1:
        case 2:
            ptr = kernel_heap_allocator_8.alloc();
            break;
        case 3:
            ptr = kernel_heap_allocator_16.alloc();
            break;
        case 4:
            ptr = kernel_heap_allocator_32.alloc();
            break;
        case 5:
            ptr = kernel_heap_allocator_64.alloc();
            break;
        case 6:
            ptr = kernel_heap_allocator_128.alloc();
            break;
        case 7:
            ptr = kernel_heap_allocator_256.alloc();
            break;
        case 8:
            ptr = kernel_heap_allocator_512.alloc();
            break;
        case 9:
            ptr = kernel_heap_allocator_1024.alloc();
            break;
        default:
            warnf("new: size %d is too large", size);
            ptr = kernel_allocator.alloc_page();
            // panic("new failed");
            break;
        }
    }

    
    

    // if (((uint64)ptr & (PGSIZE-1)) == 0) {
    //     panic("new: ptr is page-aligned");
    // }

    // debugf("new: %d, %p", size, ptr);

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
    // debugf("delete %p", ptr);

    if (((uint64)ptr & (PGSIZE-1)) == 0) {
        kernel_allocator.free_page(ptr);
        return;
    } 
    

    uint16 size = *(uint16*)((uint64)ptr & ~(PGSIZE-1));

    switch (size/8) {
        case 1:
            kernel_heap_allocator_8.free(ptr);
            break;
        case 2:
            kernel_heap_allocator_16.free(ptr);
            break;
        case 4:
            kernel_heap_allocator_32.free(ptr);
            break;
        case 8:
            kernel_heap_allocator_64.free(ptr);
            break;
        case 16:
            kernel_heap_allocator_128.free(ptr);
            break;
        case 32:
            kernel_heap_allocator_256.free(ptr);
            break;
        case 64:
            kernel_heap_allocator_512.free(ptr);
            break;
        case 128:
            kernel_heap_allocator_1024.free(ptr);
            break;
        default:
            panic("delete failed");
            break;
    }

    
}

void operator delete(void* ptr, std::size_t size) {
    (void)(size);
    operator delete(ptr);
}

void operator delete[](void* ptr) {
    operator delete(ptr);
}

void operator delete [](void* ptr, std::size_t size) {
    (void)(size);
    operator delete(ptr);

}

