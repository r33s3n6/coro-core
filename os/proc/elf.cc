#include <mm/layout.h>
#include <proc/process.h>
#include <trap/trap.h>
#include <ccore/types.h>

#include <mm/allocator.h>
#include <mm/vmem.h>



struct elf64_header {
    constexpr static uint64 ELF_MAGIC = 0x464c457f; // "\x7fELF" in little endian
    enum ei_class_t: uint8 {
        BIT32 = 1,
        BIT64 = 2,
    };
    enum ei_data_t: uint8 {
        LITTLE = 1,
        BIG    = 2,
    };
    enum ei_version_t: uint8 {
        EI_CURRENT = 1,
    };
    enum ei_osabi_t: uint8 {
        SYSTEM_V = 0,
        LINUX    = 3,
    };
    enum ei_abiversion_t: uint8 {
        ABI_CURRENT = 0,
    };

    enum e_type_t: uint16 {
        ET_NONE   = 0,
        ET_REL    = 1,
        ET_EXEC   = 2,
        ET_DYN    = 3,
        ET_CORE   = 4,
        ET_LOOS   = 0xfe00,
        ET_HIOS   = 0xfeff,
        ET_LOPROC = 0xff00,
        ET_HIPROC = 0xffff,
    };

    enum e_machine_t: uint16 {
        M32     = 1,
        SPARC   = 2,
        X86     = 3,
        MIPS    = 4,
        POWERPC = 5,
        S390    = 6,
        ARM     = 7,
        SUPERH  = 8,
        IA64    = 9,
        X86_64  = 0x3e,
        AARCH64 = 0xb7,
        RISCV   = 0xf3,
    };

    enum e_version_t: uint32 {
        E_CURRENT = 1,
    };


    uint32 ei_magic;
    ei_class_t ei_class;
    ei_data_t ei_data;
    ei_version_t ei_version;
    ei_osabi_t ei_osabi;
    ei_abiversion_t ei_abiversion;
    uint8 ei_pad[7];
    
    e_type_t e_type;
    e_machine_t e_machine;
    e_version_t e_version;

    uint64 e_entry;         // entry point
    uint64 e_phoff;         // program header offset
    uint64 e_shoff;         // section header offset
    uint32 e_flags;
    uint16 e_ehsize;        // elf header size
    uint16 e_phentsize;     // program header entry size
    uint16 e_phnum;
    uint16 e_shentsize;     // section header entry size
    uint16 e_shnum;         // section header entry count
    uint16 e_shstrndx;      // section header string table index
};

struct elf64_program_header {
    enum p_type_t: uint32 {
        PT_NULL    = 0,
        PT_LOAD    = 1,
        PT_DYNAMIC = 2,
        PT_INTERP  = 3,
        PT_NOTE    = 4,
        PT_SHLIB   = 5,
        PT_PHDR    = 6,
        PT_TLS     = 7,
        PT_LOOS    = 0x60000000,
        PT_HIOS    = 0x6fffffff,
        PT_LOPROC  = 0x70000000,
        PT_HIPROC  = 0x7fffffff,
    };
    enum p_flags_t: uint32 {
        PF_NONE = 0,
        PF_X = 1,
        PF_W = 2,
        PF_WX = 3,
        PF_R = 4,
        PF_RX = 5,
        PF_RW = 6,
        PF_RWX = 7,
        PF_MASKPROC = 0xf0000000, // reserved for processor-specific semantics
    };
    p_type_t p_type;

    p_flags_t p_flags;
    uint64 p_offset;
    uint64 p_vaddr;
    uint64 p_paddr;
    uint64 p_filesz;
    uint64 p_memsz;
    uint64 p_align;
};

struct elf64_section_header {
    enum sh_type_t: uint32 {
        SHT_NULL          = 0,
        SHT_PROGBITS      = 1,
        SHT_SYMTAB        = 2,
        SHT_STRTAB        = 3,
        SHT_RELA          = 4,
        SHT_HASH          = 5,
        SHT_DYNAMIC       = 6,
        SHT_NOTE          = 7,
        SHT_NOBITS        = 8,
        SHT_REL           = 9,
        SHT_SHLIB         = 0x0a,
        SHT_DYNSYM        = 0x0b,
        SHT_INIT_ARRAY    = 0x0e,
        SHT_FINI_ARRAY    = 0x0f,
        SHT_PREINIT_ARRAY = 0x10,
        SHT_GROUP         = 0x11,
        SHT_SYMTAB_SHNDX  = 0x12,
        SHT_LOOS          = 0x60000000,
        SHT_HIOS          = 0x6fffffff,
        SHT_LOPROC        = 0x70000000,
        SHT_HIPROC        = 0x7fffffff,
        SHT_LOUSER        = 0x80000000,
        SHT_HIUSER        = 0xffffffff,
    };

    enum sh_flags_t: uint64 {
        SHF_WRITE      = 0x1,
        SHF_ALLOC      = 0x2,
        SHF_EXECINSTR  = 0x4,
        SHF_MERGE      = 0x10,
        SHF_STRINGS    = 0x20,
        SHF_INFO_LINK  = 0x40,
        SHF_LINK_ORDER = 0x80,
        SHF_OS_NONCONFORMING = 0x100,
        SHF_GROUP      = 0x200,
        SHF_TLS        = 0x400,
        SHF_COMPRESSED = 0x800,
        SHF_MASKOS     = 0x0ff00000,
        SHF_MASKPROC   = 0xf0000000,
    };

    uint32 sh_name;         // section name
    sh_type_t sh_type;
    sh_flags_t sh_flags;
    uint64 sh_addr;
    uint64 sh_offset;
    uint64 sh_size;
    uint32 sh_link;
    uint32 sh_info;
    uint64 sh_addralign;
    uint64 sh_entsize;
};

int user_process::__load_elf(uint64 start, uint64 size) {
    // uint64 end = start + size;
    if (size < sizeof(elf64_header)) {
        return -1;
    }
    uint64 current_addr = start;
    elf64_header *header = (elf64_header *)start;
    current_addr += sizeof(elf64_header);

    // verify compatibility
    if (header->ei_magic != elf64_header::ELF_MAGIC) {
        warnf("invalid elf magic");
        return -1;
    }
    if (header->ei_class != elf64_header::BIT64) {
        warnf("invalid elf class");
        return -1;
    }
    if (header->ei_data != elf64_header::LITTLE) {
        warnf("invalid elf data");
        return -1;
    }
    if (header->ei_version != elf64_header::EI_CURRENT) {
        warnf("invalid elf version");
        return -1;
    }
    if (header->ei_osabi != elf64_header::SYSTEM_V) {
        warnf("invalid elf osabi");
        return -1;
    }
    if (header->ei_abiversion != elf64_header::ABI_CURRENT) {
        warnf("invalid elf abiversion");
        return -1;
    }
    if (header->e_type != elf64_header::ET_EXEC) {
        warnf("invalid elf type");
        return -1;
    }
    if (header->e_machine != elf64_header::RISCV) {
        warnf("invalid elf machine");
        return -1;
    }
    if (header->e_version != elf64_header::E_CURRENT) {
        warnf("invalid elf version");
        return -1;
    }
    if (header->e_phentsize != sizeof(elf64_program_header)) {
        warnf("invalid elf phentsize");
        return -1;
    }
    if (header->e_shentsize != sizeof(elf64_section_header)) {
        warnf("invalid elf shentsize");
        return -1;
    }

    // load program headers
    if (header->e_phoff == 0 || header->e_phnum == 0) {
        warnf("invalid elf program headers");
        return -1;
    }
    if (header->e_phoff + header->e_phnum * sizeof(elf64_program_header) > size) {
        warnf("invalid elf program headers");
        return -1;
    }
    elf64_program_header *ph = (elf64_program_header *)(start + header->e_phoff);
    for (uint32 i = 0; i < header->e_phnum; i++) {
        if (ph[i].p_type == elf64_program_header::PT_LOAD) {
            if (ph[i].p_offset + ph[i].p_filesz > size) {
                warnf("ph[i].p_offset + ph[i].p_filesz > size");
                return -1;
            }
            uint64 start_addr = ph[i].p_vaddr;

            // uint64 end_addr = start_addr + ph[i].p_memsz;
            uint64 start_addr_aligned = PGROUNDDOWN(start_addr);
            uint64 end_addr_aligned = PGROUNDUP(ph[i].p_vaddr + ph[i].p_memsz);
            uint64 end_addr_file = ph[i].p_vaddr + ph[i].p_filesz;

            uint64 mem_size = end_addr_aligned - start_addr_aligned;

            if (end_addr_aligned >= USER_STACK_TOP) {
                warnf("end_addr_aligned >= USER_STACK_TOP");
                return -1;
            }

            if (start_addr_aligned < USER_TEXT_START) {
                warnf("start_addr_aligned (%p) < USER_TEXT_START", start_addr_aligned);
                return -1;
            }

            if (start_addr_aligned < min_va) {
                min_va = start_addr_aligned;
            }
            if (end_addr_aligned > max_va) {
                max_va = end_addr_aligned;
            }
            
            // map memory
            uint64 pte_flags = 0;
            if (ph[i].p_flags & elf64_program_header::PF_R) {
                pte_flags |= PTE_R;
            }
            if (ph[i].p_flags & elf64_program_header::PF_W) {
                pte_flags |= PTE_W;
            }
            if (ph[i].p_flags & elf64_program_header::PF_X) {
                pte_flags |= PTE_X;
            }
            pte_flags |= PTE_U;
            int ret = __mmap(start_addr_aligned, mem_size, mmap_info::MAP_USER, pte_flags);
            if (ret < 0) {
                warnf("mmap failed");
                return -1;
            }

            // debugf("load elf: start_addr: %p, mem_size: %p, pte_flags: %p", start_addr_aligned, mem_size, pte_flags);

            // copy data
            ret = copyout(pagetable, start_addr, (void*)(start + ph[i].p_offset), ph[i].p_filesz);
            if (ret < 0) {
                warnf("copyout failed");
                return -1;
            }
            // memset zeros
            if (start_addr > start_addr_aligned) {
                ret = memset_user(pagetable, start_addr_aligned, 0, start_addr - start_addr_aligned);
                if (ret < 0) {
                    warnf("memset_user failed");
                    return -1;
                }
            }
            if (end_addr_file < end_addr_aligned) {
                ret = memset_user(pagetable, end_addr_file, 0, end_addr_aligned - end_addr_file);
                if (ret < 0) {
                    warnf("memset_user failed");
                    return -1;
                }
            }
            

        }
    }

    // set entry point
    trapframe_pa->epc = header->e_entry;
    // _state = RUNNABLE;

    return 0;

    
}