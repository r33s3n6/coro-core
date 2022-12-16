#include <cstdio>
#include <mm/vmem.h>
#include <mm/allocator.h>

#include <atomic/lock.h>

extern "C" 
{
void *malloc(size_t size);

void free(void*);

uint64 __mmap_top = MMAP_START;

void *mmap(void *, size_t length, int, int, int, off_t) {
    
    // assert addr == nullptr, prot == RW, flags == PRIVATE|ANONYMOUS
    // fd == -1, offset == 0

    // auto guard = make_lock_guard(lock);

    uint64 vaddr_start = __mmap_top;
    uint64 vaddr = vaddr_start;
    int page_count = PGROUNDUP(length) / PGSIZE;


    int mapped_pages = 0;
    for (;mapped_pages<page_count;mapped_pages++) {
        void* page = kernel_allocator.alloc_page();
        if (page == nullptr) {
            goto fail;
        }
        kvmmap(kernel_pagetable, vaddr, (uint64)page, PGSIZE, PTE_W|PTE_R|PTE_X);
        vaddr += PGSIZE;
    }

    __mmap_top += page_count * PGSIZE;
    memset((void*)vaddr_start, 0, length);
    // debugf("mmap %p (%l)", vaddr_start, length);
    return (void*)vaddr_start;


fail: // unmap and free pages
    if (mapped_pages > 0) {
        kvmunmap(kernel_pagetable, vaddr_start, mapped_pages*PGSIZE, 1);
    }

    return nullptr;


}

int munmap(void *addr, size_t length) {
    kvmunmap(kernel_pagetable, (uint64)addr, length, 1);
    return 0;
}

static int __errno_arr[NCPU] {0};

int *__errno (void) {
    return __errno_arr+cpu::current_id();
}

struct _reent;

struct _reent *_impure_ptr;

int atoi(const char *__nptr) {
    (void)(__nptr);
    return 0;
}

char *getenv(const char *__string) {
    (void)(__string);
    return nullptr;
}

int fprintf(FILE *, const char * fmt, ...) {
    debugf("fprintf: fmt: %s", fmt);
    return 1;
}


}

static spinlock ptmalloc_lock {"ptmalloc_lock"};

void* ptmalloc_malloc(size_t size) {
    auto guard = make_lock_guard(ptmalloc_lock);
    return malloc(size);
}

void ptmalloc_free(void* ptr) {
    auto guard = make_lock_guard(ptmalloc_lock);
    free(ptr);
}