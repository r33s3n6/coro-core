#include <mm/layout.h>
#include <proc/process.h>
#include <trap/trap.h>
#include <ccore/types.h>

#include <mm/allocator.h>
#include <mm/vmem.h>

static int app_cur, app_num;
static uint64 *app_info_ptr;
extern char _app_num[], _app_names[];
// const uint64 BASE_ADDRESS = 0x1000; // user text start
#define APP_NAME_MAX 100
#define APP_MAX_CNT 50
char names[APP_MAX_CNT][APP_NAME_MAX];

void init_app_names()
{
    char *s;
    app_info_ptr = (uint64 *)_app_num;
    app_cur = -1;
    app_num = *app_info_ptr;
    app_info_ptr++;

    s = _app_names;
    for (int i = 0; i < app_num; ++i)
    {
        int len = strlen(s);
        strncpy(names[i], (const char *)s, len);
        s += len + 1;
        printf("app name: %s\n", names[i]);
    }
}

int get_app_id_by_name(char *name)
{
    for (int i = 0; i < app_num; ++i)
    {
        if (strncmp(name, names[i], APP_NAME_MAX) == 0)
            return i;
    }
    return -1;
}


int user_process::__load_text(uint64 start, uint64 end) {
    debugf("load range = [%p, %p)", start, end);
    uint64 s = PGROUNDDOWN(start), e = PGROUNDUP(end);
    uint64 va, pa;

    for (va = USER_TEXT_START, pa = s; pa < e; va += PGSIZE, pa += PGSIZE) {
        void *page = kernel_allocator.alloc_page();
        if (page == nullptr) {
            break;
        }
        memmove(page, (const void *)pa, PGSIZE);
        if (uvmmap(pagetable, va, (uint64)page, PGSIZE, PTE_U | PTE_R | PTE_X) != 0) {
            kernel_allocator.free_page(page);
            break;
        }
    }

    if (pa != e) {
        // failed, goto cleanup
        goto err_unmap;
    }

    trapframe_pa->epc = USER_TEXT_START;
    text_size = e - s;
    return 0;

err_unmap:
    uint64 free_size = va - USER_TEXT_START - PGSIZE;
    if (free_size > 0) {
        uvmunmap(pagetable, USER_TEXT_START, free_size, 1);
    }
    return -ENOMEM;
}




void loader(int id, struct proc *p) {
    infof("loader %s", names[id]);
    bin_loader(app_info_ptr[id], app_info_ptr[id + 1], p);
}

// load shell from kernel data section
// and make it a proc
int make_shell_proc()
{
    struct proc *p = alloc_proc();

    // still need to init: 
    //  * parent           
    //  * ustack_bottom    
    //  * total_size       
    //  * cwd              
    //  * name
    //  * next_shmem_addr             

    // parent
    p->parent = nullptr;

    int id = get_app_id_by_name( "shell" );
    if (id < 0)
        panic("no user shell");
    loader(id, p);  // will fill ustack_bottom, next_shmem_addr and total_size

    // name
    safestrcpy(p->name, "shell", PROC_NAME_MAX);

    // cwd
    p->cwd = inode_by_name("/");
    p->state = RUNNABLE;
    release(&p->lock);

    return 0;
}
