
#include <mm/layout.h>
#include <utils/log.h>
#include <utils/assert.h>
// #include <proc/proc.h>

#include <mm/vmem.h>
#include <mm/allocator.h>

pagetable_t kernel_pagetable;



// Make a direct-map page table for the kernel.
pagetable_t kvmmake() {
    pagetable_t kpgtbl;

    kpgtbl = (pagetable_t)kernel_allocator.alloc_page();
    if(!kpgtbl) {
        panic("kvmmake");
    }

    memset(kpgtbl, 0, PGSIZE);

    // uart registers
    // kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

    // virtio mmio disk interface
    debugf("virtio_disk: va = %p pa = %p", VIRTIO0, VIRTIO0);
    kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

    // PLIC
    debugf("plic:        va = %p pa = %p", PLIC, PLIC);
    kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

    // map kernel text executable and read-only.
    debugf("kernel text: (va) %p -> [%p, %p)", KERNBASE, KERNBASE, (uint64)e_text);
    kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)e_text - KERNBASE, PTE_R | PTE_X);


    // map kernel data and the physical RAM we'll make use of.
    debugf("kernel data: (va) %p -> [%p, %p)", e_text, e_text, IO_MEM_START);
    kvmmap(kpgtbl, (uint64)e_text, (uint64)e_text, IO_MEM_START - (uint64)e_text, PTE_R | PTE_W);


    debugf("io memory:   (va) %p -> [%p, %p)", IO_MEM_START, IO_MEM_START, PHYSTOP);
    kvmmap(kpgtbl, IO_MEM_START, IO_MEM_START, PHYSTOP - IO_MEM_START, PTE_R | PTE_W | PTE_MT_IO);


    // map trampoline
    debugf("trampoline:  (va) %p -> [%p, %p)", TRAMPOLINE, trampoline, trampoline + PGSIZE);
    kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

    return kpgtbl;
}

// Initialize the one kernel_pagetable
// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvminit() {
    // generate the kernel page table
    kernel_pagetable = kvmmake();
}

// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvminithart() {
    // enable virtual memory
    w_satp(MAKE_SATP(kernel_pagetable));

    // update TLB
    sfence_vma();

    infof("enable paging, satp: %p", r_satp());
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc) {
    if (va >= MAXVA)
        panic("walk");

    for (int level = 2; level > 0; level--)
    {
        pte_t *pte = &pagetable[PX(level, va)];
        if (*pte & PTE_V) {
            // found the pte
            pagetable = (pagetable_t)PTE2PA(*pte);
        } else {
            // pte does not exist
            if (!alloc) {
                return nullptr;
            }
            // should create new child page table
            pagetable = (pagetable_t)kernel_allocator.alloc_page();
            if (pagetable == nullptr) {
                warnf("out of memory when creating pagetable");
                return nullptr;
            }

            // create new child pagetable successfully
            // point to it
            memset(pagetable, 0, PGSIZE);
            *pte = PA2PTE(pagetable) | PTE_V;
        }
    }
    return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64 walkaddr(pagetable_t pagetable, uint64 va) {
    pte_t *pte;
    uint64 pa;

    if (va >= MAXVA)
        return 0;

    pte = walk(pagetable, va, false);
    if (pte == 0)
        return 0;
    if ((*pte & PTE_V) == 0)
        return 0;
    if ((*pte & PTE_U) == 0)
        return 0;
    pa = PTE2PA(*pte);
    return pa;
}



// Look up a virtual address, return the physical address,
uint64 virt_addr_to_physical(pagetable_t pagetable, uint64 va) {
    uint64 page = walkaddr(pagetable, va);
    if (page == 0)
        return 0;
    return page | (va & 0xFFFULL);
}



// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size, uint64 perm) {
    uint64 a, last;
    pte_t *pte;

    // debugf("va=%p->pa=%p, size=%p, UXWR=%d%d%d%d", va, pa, size,
    //        HAS_BIT(perm, PTE_U),
    //        HAS_BIT(perm, PTE_X),
    //        HAS_BIT(perm, PTE_W),
    //        HAS_BIT(perm, PTE_R));
    a = PGROUNDDOWN(va);
    last = PGROUNDDOWN(va + size - 1);
    // int cnt =0 ;
    for (;;)
    {
        // cnt+=1;
        if ((pte = walk(pagetable, a, 1)) == 0)
            return -1;
        if (*pte & PTE_V)
            panic("remap");
        *pte = PA2PTE(pa) | perm | PTE_V;
        if (a == last)
            break;
        a += PGSIZE;
        pa += PGSIZE;
    }
    // debugf("map page cnt=%d", cnt);
    return 0;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, uint64 perm) {
    if (mappages(kpgtbl, va, pa, sz, perm) != 0)
        panic("kvmmap");
}

int uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size, uint64 perm) {
    return mappages(pagetable, va, pa, size, perm);
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int map1page(pagetable_t pagetable, uint64 va, uint64 pa, int perm) {
    uint64 a;
    pte_t *pte;
    uint64 size = PGSIZE;
    // debugf("va=%p->pa=%p, size=%p, UXWR=%d%d%d%d", va, pa, size,
    //        HAS_BIT(perm, PTE_U),
    //        HAS_BIT(perm, PTE_X),
    //        HAS_BIT(perm, PTE_W),
    //        HAS_BIT(perm, PTE_R));
    a = PGROUNDDOWN(va);

    if ((pte = walk(pagetable, a, true)) == 0)
        return -1;
    if (*pte & PTE_V)
    {
        infof("map1page: remap\n");
        return -2;
    }
    *pte = PA2PTE(pa) | perm | PTE_V;

    return size;
}

// Remove npages of mappings starting from va. va might not be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free) {
    uint64 a, last;
    pte_t *pte;

    if(((va % PGSIZE) != 0) || ((size % PGSIZE) != 0)) {
        warnf("va=%p size=%l do_free=%d", va, size, do_free);
        panic("uvmunmap: not aligned");
    }

    a = va;
    last = PGROUNDDOWN(va + size - 1);


    debugf("uvmunmap: va=%p size=%d do_free=%d", va, size, do_free);

    for (;;)
    {

        if ((pte = walk(pagetable, a, false)) == 0)
            panic("uvmunmap: walk");
        if ((*pte & PTE_V) == 0)
            panic("uvmunmap: not mapped");
        if (PTE_FLAGS(*pte) == PTE_V)
            panic("uvmunmap: not a leaf");
        if (do_free)
        {
            uint64 pa = PTE2PA(*pte);
            kernel_allocator.free_page((void *)pa);
        }
        *pte = 0;

        if (a == last)
            break;
        a += PGSIZE;

    }

}

void kvmunmap(pagetable_t kpgtbl, uint64 va, uint64 size, int do_free) {
    uvmunmap(kpgtbl, va, size, do_free);
}

// create an empty user page table.
// map trampoline page
// returns 0 if out of memory.
pagetable_t create_empty_user_pagetable() {
    pagetable_t pagetable;
    pagetable = (pagetable_t)kernel_allocator.alloc_page();
    if (pagetable != nullptr) {
        memset(pagetable, 0, PGSIZE);
    }
    return pagetable;
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
    char *mem;
    uint64 a;

    if (newsz < oldsz)
        return oldsz;

    oldsz = PGROUNDUP(oldsz);
    for (a = oldsz; a < newsz; a += PGSIZE)
    {
        mem = (char*)kernel_allocator.alloc_page();
        if (mem == nullptr) {
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
        memset(mem, 0, PGSIZE);
        if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U) != 0)
        {
            kernel_allocator.free_page(mem);
            uvmdealloc(pagetable, a, oldsz);
            return 0;
        }
    }
    return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz) {
    if (newsz >= oldsz)
        return oldsz;

    if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
    {
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, true);
    }

    return newsz;
}

// Recursively free page-table pages.
// All leaf mappings must already have been removed.
void free_pagetable_pages(pagetable_t pagetable) {
    if(!pagetable)
        return;

    // there are 2^9 = 512 PTEs in a page table.
    for (int i = 0; i < 512; i++)
    {
        pte_t pte = pagetable[i];
        if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
        {
            // this PTE points to a lower-level page table.
            uint64 child = PTE2PA(pte);
            free_pagetable_pages((pagetable_t)child);
            pagetable[i] = 0;
        }
        else if (pte & PTE_V)
        {
            panic("free_pagetable_pages is containing a valid page");
        }
    }
    kernel_allocator.free_page(pagetable);
}

/*
// Free user memory pages,
// then free page-table pages.
// total_size used for checking
void free_user_mem_and_pagetables(pagetable_t pagetable, uint64 total_size) {

    // free ustack
    debug_core("free_user_mem_and_pagetables free stack");
    uvmunmap(pagetable, USER_STACK_BOTTOM - USTACK_SIZE, USTACK_SIZE, true);
    total_size -= USTACK_SIZE;
    
    // free bin
    debug_core("free_user_mem_and_pagetables free bin");
    uvmunmap(pagetable, USER_TEXT_START, PGROUNDUP(total_size), true);
    total_size -= PGROUNDUP(total_size);

    kernel_assert(total_size==0, "");

    // free page-table pages
    free_pagetable_pages(pagetable);
}*/

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va) {
    pte_t *pte;

    pte = walk(pagetable, va, false);
    if (pte == 0)
        panic("uvmclear");
    *pte &= ~PTE_U;
}

int uvmcopy(pagetable_t old_pagetable, pagetable_t new_pagetable, uint64 total_size) {
    pte_t *pte;
    uint64 pa, cur_addr;
    uint flags;
    char *mem;
    // debugcore("to copy ustack, sz=%d", total_size);
    // copy ustack
    for (cur_addr = USER_STACK_BOTTOM - USTACK_SIZE; cur_addr < USER_STACK_BOTTOM; cur_addr += PGSIZE)
    {
        if ((pte = walk(old_pagetable, cur_addr, false)) == 0)
            panic("uvmcopy: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = (char*)kernel_allocator.alloc_page()) == nullptr)
            goto err_ustack;
        memmove(mem, (char *)pa, PGSIZE);
        if (mappages(new_pagetable, cur_addr, PGSIZE, (uint64)mem, flags) != 0)
        {
            kernel_allocator.free_page(mem);
            goto err_ustack;
        }
    }

    total_size -= USTACK_SIZE;
    // debugcore("to copy bin, sz=%d", total_size);
    // free any other
    for (cur_addr = USER_TEXT_START; cur_addr < USER_TEXT_START+total_size; cur_addr += PGSIZE)
    {
        if ((pte = walk(old_pagetable, cur_addr, false)) == 0)
            panic("uvmcopy: pte should exist");
        if ((*pte & PTE_V) == 0)
            panic("uvmcopy: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte);
        if ((mem = (char*)kernel_allocator.alloc_page()) == nullptr)
            goto err;
        memmove(mem, (char *)pa, PGSIZE);
        if (mappages(new_pagetable, cur_addr, PGSIZE, (uint64)mem, flags) != 0)
        {
            kernel_allocator.free_page(mem);
            goto err;
        }
    }
    return 0;

err_ustack:
    debug_core("Copy ustack error");
    uvmunmap(new_pagetable, USER_STACK_BOTTOM - USTACK_SIZE, USTACK_SIZE / PGSIZE, true);
    return -1;

err:
    debug_core("Copy user space error");
    uvmunmap(new_pagetable, USER_TEXT_START, (cur_addr - USER_TEXT_START) / PGSIZE, false);
    return -1;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int copyout(pagetable_t pagetable, uint64 dstva, void *src, uint64 len) {
    uint64 n, va0, pa0;

    char* _src = (char*)src;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (dstva - va0);
        if (n > len)
            n = len;
        memmove((void *)(pa0 + (dstva - va0)), _src, n);

        len -= n;
        _src += n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

int memset_user(pagetable_t pagetable, uint64 dstva, uint8 val, uint64 len) {
    uint64 n, va0, pa0;

    while (len > 0) {
        va0 = PGROUNDDOWN(dstva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (dstva - va0);
        if (n > len)
            n = len;
        memset((void *)(pa0 + (dstva - va0)), val, n);

        len -= n;
        dstva = va0 + PGSIZE;
    }
    return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int copyin(pagetable_t pagetable, void *dst, uint64 srcva, uint64 len) {
    uint64 n, va0, pa0;
    char* _dst = (char*)dst;

    while (len > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0)
            return -1;
        n = PGSIZE - (srcva - va0);
        if (n > len)
            n = len;
        memmove(_dst, (void *)(pa0 + (srcva - va0)), n);

        len -= n;
        _dst += n;
        srcva = va0 + PGSIZE;
    }
    return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max) {
    uint64 n, va0, pa0;
    int got_null = 0;

    while (got_null == 0 && max > 0) {
        va0 = PGROUNDDOWN(srcva);
        pa0 = walkaddr(pagetable, va0);
        if (pa0 == 0) {
            debug_core("bad addr");
            return -1;
        }
        n = PGSIZE - (srcva - va0);
        if (n > max)
            n = max;

        char *p = (char *)(pa0 + (srcva - va0));
        while (n > 0) {
            if (*p == '\0') {
                *dst = '\0';
                got_null = 1;
                break;
            } else {
                *dst = *p;
            }
            --n;
            --max;
            p++;
            dst++;
        }

        srcva = va0 + PGSIZE;
    }
    if (got_null) {
        return 0;
    } else {
        debug_core("no null");
        return -1;
    }
}

/*
// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int64 either_copyout(void *dst, void *src, uint64 len, int is_user_dst) {
    struct proc *p = curr_proc();
    if (is_user_dst) {
        return copyout(p->pagetable, (uint64)dst, src, len);
    } else {
        memmove(dst, src, len);
        return 0;
    }
}

// Copy from either a user address, or kernel address,
// depending on is_user_src.
// Returns 0 on success, -1 on error.
int64 either_copyin(void *dst, void *src, uint64 len, int is_user_src) {
    struct proc *p = curr_proc();
    if (is_user_src) {
        return copyin(p->pagetable, dst, (uint64)src, len);
    } else {
        memmove(dst, src, len);
        return 0;
    }
}

*/
