// provide new/delete operators for the kernel
#ifndef MM_ALLOCATOR_H
#define MM_ALLOCATOR_H

#include <new>
#include <atomic/lock.h>
#include <arch/riscv.h>
#include <utils/log.h>

#include "utils.h"

// heap allocator
void* operator new(std::size_t size);

void* operator new[](std::size_t size);

void* operator new(std::size_t size, const std::nothrow_t&) noexcept;

void operator delete(void* ptr);

void operator delete(void* ptr, std::size_t size);

void operator delete[](void* ptr);

void check_memory(); // debug

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

    public:
    allocator(void* start, void* end){
        this->start = (void*)PGROUNDUP((uint64)start);
        this->end = (void*)PGROUNDDOWN((uint64)end);
        for (char* p = (char*)this->start; p < (char*)this->end; p += PGSIZE){
            free_page(p);
        }
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
        }
        

        #ifdef MEMORY_DEBUG
        memset((char*)pa + sizeof(page_info), 0x5a, PGSIZE - sizeof(page_info));
        #endif
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
        }

        #ifdef MEMORY_DEBUG
        memset(page, 0x5c, PGSIZE);
        #endif

        return page;
    }

    uint64 get_free_page_count() const {
        return free_page_count;
    }


};

extern allocator kernel_allocator;


#endif // MM_ALLOCATOR_H