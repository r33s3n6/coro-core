// provide new/delete operators for the kernel
#ifndef MM_ALLOCATOR_H
#define MM_ALLOCATOR_H

#include <new>
#include <atomic/lock.h>
#include <arch/riscv.h>
#include <utils/log.h>
#include <utils/list.h>
#include <utils/bitmap.h>

#include "utils.h"

// heap allocator
void* operator new(std::size_t size);

void* operator new[](std::size_t size);

void* operator new(std::size_t size, const std::nothrow_t&) noexcept;

void operator delete(void* ptr);

void operator delete(void* ptr, std::size_t size);

void operator delete[](void* ptr);

void check_memory(); // debug



extern bool use_ptmalloc;
extern bool heap_debug_output;

struct page_info {
    page_info* next_free_page = nullptr;
};

class allocator {


    private:
    page_info* free_page_list = nullptr;
    spinlock lock;
    uint64 free_page_count = 0;

    void* start = nullptr;
    void* end = nullptr;
    bool debug_output = false;

    public:
    allocator(void* start, void* end){
        this->start = (void*)PGROUNDUP((uint64)start);
        this->end = (void*)PGROUNDDOWN((uint64)end);
        for (char* p = (char*)this->start; p < (char*)this->end; p += PGSIZE){
            free_page(p);
        }
    }
    

    void set_debug(bool debug){

        debug_output = debug;
    }

    void free_page(void* pa){
        if (!pa) {
            return;
        }

        if (((uint64)pa % PGSIZE) != 0 || (pa < start || pa >= end)) {
            panic("free_page");
        }


        {
            auto guard = make_lock_guard(lock);

            page_info* page = (page_info*)pa;

            page->next_free_page = free_page_list;
            free_page_list = page;
            free_page_count++;

            #ifdef MEMORY_DEBUG
            if(debug_output){
                debug_core("free_page: %p", pa);
            }
            memset((char*)pa + sizeof(page_info), 0x5a, PGSIZE - sizeof(page_info));
            if(debug_output){
                debug_core("free_page: %p done", pa);
            }
            #endif
        }
        

        

    }

    void* alloc_page(){
        page_info* page;

        {
            auto guard = make_lock_guard(lock);

            if (free_page_list == nullptr) {

                warnf("alloc_page failed");
                return nullptr;
            }
            page = free_page_list;
            free_page_list = page->next_free_page;
            free_page_count--;

            #ifdef MEMORY_DEBUG
            if(debug_output){
                debug_core("alloc_page: %p", page);
            }
            memset(page, 0x5c, PGSIZE);
            #endif
        }


        return page;
    }

    uint64 get_free_page_count() const {
        return free_page_count;
    }


};

extern allocator kernel_allocator;
extern allocator kernel_io_allocator;

template <int size>
class heap_allocator;

template <int size>
struct heap_block_t {
    
    struct __attribute__((aligned(size))) __heap_block_info_t {
        uint16 _size = size;
        uint16 used = 0;
        uint32 next_free_index = 0;
        heap_block_t* next_block = nullptr;
        heap_block_t* next_free_block = nullptr;
        #ifdef MEMORY_DEBUG
        uint8 free_map[PGSIZE/size/8];
        #endif

    } info;
    static constexpr int capacity = (PGSIZE - sizeof(__heap_block_info_t)) / size;
    uint8 data[capacity][size] {0};

    heap_block_t() {
        for (int i=0;i<capacity-1;i++) {
            (*(uint32*)data[i]) = i+1;
        }

        (*(uint32*)data[capacity-1]) = -1;
        #ifdef MEMORY_DEBUG
        memset(info.free_map, 0, sizeof(info.free_map));
        #endif
    }

    void* alloc() {
        if (full()) {
            return nullptr;
        }
        void* ret = data[info.next_free_index];
        #ifdef MEMORY_DEBUG
        bitmap_set((uint64*)info.free_map, info.next_free_index);
        #endif

        info.next_free_index = (*(uint32*)ret);
        info.used++;

        return ret;
    }

    void free(void* ptr) {
        uint32 index = ((uint8*)ptr - (uint8*)data) / size;
        if (index >= capacity) {
            panic("free");
        }
        #ifdef MEMORY_DEBUG
        if (!bitmap_get((uint64*)info.free_map, index)) {
            debugf("free: %p, index: %d", ptr, index);
            panic("double free || free unallocated memory");
        }
        bitmap_clear((uint64*)info.free_map, index);
        #endif

        (*(uint32*)ptr) = info.next_free_index;
        info.next_free_index = index;
        info.used--;
    }

    bool full() {
        return info.used == capacity;
    }

    bool empty() {
        return info.used == 0;
    }

    uint16 get_used() const {
        return info.used;
    }
};


template <int size>
class heap_allocator {
   public:
    heap_allocator(){
        static_assert((size >= 8) && (size % 8 ==0) && (size <= 1024), "heap allocator size must be 8~1024 and multiple of 8");
    }
    void* alloc() {
        auto guard = make_lock_guard(lock);
        if (next_free_block == nullptr) {
            // alloc page
            next_free_block = (heap_block_t<size>*)kernel_allocator.alloc_page();
            if (next_free_block == nullptr) {
                return nullptr;
            }
            allocated += PGSIZE;

            // placement new
            new (next_free_block) heap_block_t<size>();

            // place it to the list
            next_free_block->info.next_block = head_block;
            head_block = next_free_block;
        }

        // there is free block
        void* ret = next_free_block->alloc();

        if (next_free_block->full()) {
            next_free_block = next_free_block->info.next_free_block;
        }

        
        used += size;

        return ret;

    }
    void free(void* ptr) {
        auto guard = make_lock_guard(lock);
        heap_block_t<size>* block = (heap_block_t<size>*)((uint64)ptr & ~(PGSIZE-1));
        bool full = block->full();
        block->free(ptr);

        if (full) {
            // add to free list
            block->info.next_free_block = next_free_block;
            next_free_block = block;
        }

        used -= size;

    }

    void truncate() {
        auto guard = make_lock_guard(lock);
        heap_block_t<size>* block = head_block;
        heap_block_t<size>* prev = nullptr;

        #ifdef MEMORY_DEBUG
        uint32 inefficient_blocks = 0;
        uint32 total_blocks = 0;
        uint32 freed_blocks = 0;
        #endif

        while (block) {
            heap_block_t<size>* next_block = block->info.next_block;
            if (block->empty()) {
                
                if (prev) {
                    prev->info.next_block = next_block;
                } else {
                    head_block = next_block;
                }
                kernel_allocator.free_page(block);
                allocated -= PGSIZE;
                #ifdef MEMORY_DEBUG
                freed_blocks++;
                #endif
            } else {
                prev = block;
                #ifdef MEMORY_DEBUG
                if (block->get_used() < heap_block_t<size>::capacity * 0.1) {
                    inefficient_blocks++;
                }
                #endif
            }

            #ifdef MEMORY_DEBUG
            total_blocks++;
            #endif

            block = next_block;
            
        }

        #ifdef MEMORY_DEBUG
        // if (inefficient_blocks)
        //     debugf("truncate %d: freed %d, inefficient %d, total %d", size, freed_blocks, inefficient_blocks, total_blocks);
        #endif
    }

    

    private:
    heap_block_t<size>* head_block = nullptr;
    heap_block_t<size>* next_free_block = nullptr;
    spinlock lock;
    public:
    uint64 allocated = 0;
    uint64 used = 0;
};




struct large_mem_info {
    uint64 page_count;
    uint64 requested_size;
};

class large_mem_allocator {
    public:
    large_mem_allocator(){
    }
    void* alloc(int size);
    void free(void* ptr);

    private:
    uint64 vmem_top = VMEM_START;
    spinlock lock {"large_mem_allocator.lock"};
};


#endif // MM_ALLOCATOR_H