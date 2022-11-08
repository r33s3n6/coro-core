#include <ccore/types.h>
#include <arch/riscv.h>

extern pagetable_t kernel_pagetable;

pagetable_t kvmmake();
void kvminit();
void kvminithart();
pte_t *walk(pagetable_t pagetable, uint64 va, int alloc);
uint64 walkaddr(pagetable_t pagetable, uint64 va);
uint64 virt_addr_to_physical(pagetable_t pagetable, uint64 va);
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, uint64  perm);
int uvmmap(pagetable_t pagetable, uint64 va, uint64 pa, uint64 size, uint64  perm);
// int map1page(pagetable_t pagetable, uint64 va, uint64 pa, int perm);
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 size, int do_free);
pagetable_t create_empty_user_pagetable();
uint64 uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
uint64 uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz);
void free_pagetable_pages(pagetable_t pagetable);
void free_user_mem_and_pagetables(pagetable_t pagetable, uint64 total_size);
void uvmclear(pagetable_t pagetable, uint64 va);
int uvmcopy(pagetable_t old_pagetable, pagetable_t new_pagetable, uint64 total_size);
int copyout(pagetable_t pagetable, uint64 dstva, void *src, uint64 len);
int copyin(pagetable_t pagetable, void *dst, uint64 srcva, uint64 len);
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);
int64 either_copyout(void *dst, void *src, uint64 len, int is_user_dst);
int64 either_copyin(void *dst, void *src, uint64 len, int is_user_src);