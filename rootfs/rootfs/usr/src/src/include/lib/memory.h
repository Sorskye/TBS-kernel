#ifndef MEMORY_H
#define MEMORY_H


#include "types.h"
#define MULTIBOOT_MEMORY_AVAILABLE 1

struct multiboot_mmap_entry
{
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed));

struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
};

struct memory_entry{
    uint32_t addr;
    uint32_t len;
};

typedef struct memory_block {
    uint32_t len;                // total block size INCLUDING this header; LSB used as "used" flag
    struct memory_block* next;   // valid only when block is free (i.e. used bit == 0)
} memory_block_t;


typedef struct mem_inf{
    memory_block_t* memory_block_list;
    uint32_t total_usable_memory;
    uint32_t entries;
} mem_inf_t;

typedef struct multiboot_module {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;


void parse_memory_map(struct multiboot_info* mbinfo);

void* kmalloc(uint32_t size);

void kfree(void*ptr);

void* memcpy(void* dest, const void* src, size_t n);
void* memmove(void* dest, const void* src, size_t n);
void* memset(void *b, int c, int len);
void debug_block_info(void *user_ptr);
void *bump_alloc(size_t n);

bool vmm_check_identity_range(uint32_t start, uint32_t end);
bool vmm_check_identity_page(uint32_t addr);
void debug_module_pte(uint32_t phys_addr, struct multiboot_info* mbinfo);
void verify_page_table_integrity(void);

#endif