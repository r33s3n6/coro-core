#include <utils/printf.h>
#include <utils/panic.h>
#include <utils/log.h>
#include <utils/assert.h>
#include <utils/backtrace.h>

#include <map>

#include "allocator.h"
#include "vmem.h"
#include "ptmalloc.h"

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

large_mem_allocator kernel_large_mem_allocator;

bool use_ptmalloc = false;
bool heap_debug_output = false;

class memory_statistics {
    uint64 requested = 0;
    uint64 occupied = 0;
    public:
    void alloc(uint64 requested_size, uint64 occupied_size) {
        requested += requested_size;
        occupied += occupied_size;
    }

    void free(uint64 requested_size, uint64 occupied_size){
        requested -= requested_size;
        occupied -= occupied_size;
    }

    void print() {
        debugf("memory allocation: requested: %l KiB (%l), occupied: %l KiB (%l)",
         requested/1024, requested, occupied/1024, occupied);
        // debugf("    heap<8>   : allocated: %l KiB, used %l KiB", kernel_heap_allocator_8   .allocated/1024, kernel_heap_allocator_8   .used/1024);
        // debugf("    heap<16>  : allocated: %l KiB, used %l KiB", kernel_heap_allocator_16  .allocated/1024, kernel_heap_allocator_16  .used/1024);
        // debugf("    heap<32>  : allocated: %l KiB, used %l KiB", kernel_heap_allocator_32  .allocated/1024, kernel_heap_allocator_32  .used/1024);
        // debugf("    heap<64>  : allocated: %l KiB, used %l KiB", kernel_heap_allocator_64  .allocated/1024, kernel_heap_allocator_64  .used/1024);
        // debugf("    heap<128> : allocated: %l KiB, used %l KiB", kernel_heap_allocator_128 .allocated/1024, kernel_heap_allocator_128 .used/1024);
        // debugf("    heap<256> : allocated: %l KiB, used %l KiB", kernel_heap_allocator_256 .allocated/1024, kernel_heap_allocator_256 .used/1024);
        // debugf("    heap<512> : allocated: %l KiB, used %l KiB", kernel_heap_allocator_512 .allocated/1024, kernel_heap_allocator_512 .used/1024);
        // debugf("    heap<1024>: allocated: %l KiB, used %l KiB", kernel_heap_allocator_1024.allocated/1024, kernel_heap_allocator_1024.used/1024);

    }
};

static memory_statistics kernel_memory_statistics;

#ifdef MEMORY_DEBUG
#define TRACE_ALLOC(ptr, req, real) \
    do { \
        if (ptr) { \
            if (heap_debug_output) { \
                debugf("alloc: %l(%l) -> %p", (uint64)(req), (uint64)(real), (ptr)); \
                /*print_backtrace();*/ \
            } \
            kernel_memory_statistics.alloc((real), (real)); \
        } else { \
            errorf("request %l(%l) failed", (uint64)(req), (uint64)(real)); \
        } \
    } while (0)
#define TRACE_FREE(req, real) \
    do { \
        if (heap_debug_output) debugf("free: %l(%l)", (uint64)(req), (uint64)(real)); \
        kernel_memory_statistics.free((req), (real)); \
    } while (0)


#else
#define TRACE_ALLOC(ptr, req, real)
#define TRACE_FREE(req, real)

#endif



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

void* __alloc(std::size_t size) {
    void* ptr = nullptr;

    if(size > 1024) {
        warnf("operator new: size: %d", size);
        ptr = kernel_large_mem_allocator.alloc(size);

    } else if (size == 1) {
        ptr = kernel_heap_allocator_8.alloc();
        TRACE_ALLOC(ptr, size, 8);
    } else {
        int l = log2_64(size-1);
        switch (l) {
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
            panic("log2_64(size-1)");
            break;
        }

        if (l<=2) {
            TRACE_ALLOC(ptr, size, 8);
        } else {
            TRACE_ALLOC(ptr, size, 2<<l);
        }

    }

    return ptr;
}

void* default_alloc(std::size_t size) {
    void* ptr = __alloc(size);
    if (!ptr) {
        warnf("try to truncate heap allocator");
        kernel_heap_allocator_8   .truncate();
        kernel_heap_allocator_16  .truncate();
        kernel_heap_allocator_32  .truncate();
        kernel_heap_allocator_64  .truncate();
        kernel_heap_allocator_128 .truncate();
        kernel_heap_allocator_256 .truncate();
        kernel_heap_allocator_512 .truncate();
        kernel_heap_allocator_1024.truncate();
        
        ptr = __alloc(size);
        if (!ptr) {
            warnf("allocation failed after truncate");
            kernel_memory_statistics.print();
        }

        
    }

    return ptr;
}

// TODO: failed when no memory
void* operator new(std::size_t size) {
    if (use_ptmalloc)
        return ptmalloc_malloc(size);
    else
        return default_alloc(size);

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

void default_delete(void* ptr) {
    if ((uint64)(ptr) >= VMEM_START) {
        kernel_large_mem_allocator.free(ptr);
        return;
    } else {
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
                warnf("ptr: %p, broken size: %d", ptr, size);
                panic("delete failed");
                break;
        }

        TRACE_FREE(size, size); // TODO: we cannot trace small allocation now
    }

    
}

void operator delete(void* ptr) {
    // debugf("delete %p", ptr);
    if ((uint64)(ptr) >= MMAP_START)
        ptmalloc_free(ptr);
    else
        default_delete(ptr);
    

    
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


void* large_mem_allocator::alloc(int size) {

    auto guard = make_lock_guard(lock);
    uint64 vaddr_start = vmem_top;
    uint64 vaddr = vaddr_start;
    int page_count = (size + sizeof(large_mem_info) + PGSIZE - 1) / PGSIZE;
    large_mem_info* info = (large_mem_info*)vaddr_start;
    void* ptr = nullptr;
    int mapped_pages = 0;
    for (;mapped_pages<page_count;mapped_pages++) {
        void* page = kernel_allocator.alloc_page();
        if (page == nullptr) {
            goto fail;
        }
        kvmmap(kernel_pagetable, vaddr, (uint64)page, PGSIZE, PTE_W|PTE_R|PTE_X);
        vaddr += PGSIZE;
    }

    vmem_top += page_count * PGSIZE;

    info->page_count = page_count;
    info->requested_size = size;
    ptr = (info+1);

    TRACE_ALLOC(ptr, size, page_count * PGSIZE);

    return ptr;


fail: // unmap and free pages
    if (mapped_pages > 0) {
        kvmunmap(kernel_pagetable, vaddr_start, mapped_pages*PGSIZE, 1);
    }
    TRACE_ALLOC(nullptr, size, page_count * PGSIZE);
    return nullptr;
        

}

void large_mem_allocator::free(void* ptr) {
    // get info
    auto guard = make_lock_guard(lock);
    large_mem_info* info = (large_mem_info*)ptr;
    info-=1;
    kernel_assert(((uint64)info & (PGSIZE-1)) == 0, "large_mem_allocator::free: info is not page-aligned");
    kernel_assert((uint64)info >= VMEM_START, "large_mem_allocator::free: info is not in kernel virtual map section");

    kvmunmap(kernel_pagetable, (uint64)info, info->page_count * PGSIZE, 1);

    TRACE_FREE(info->requested_size, info->page_count * PGSIZE);

}
