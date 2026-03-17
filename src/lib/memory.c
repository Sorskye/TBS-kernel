#include "types.h"
#include "stdio.h"
#include "string.h"
#include "vga-textmode.h"
#include "serial.h"

#include "kerror.h"
#include "memory.h"

#define ALIGN4(x)   (((x) + 3) & ~3U)
#define USED_FLAG   1U
#define MAX_MEM_ENTRIES 128
#define PAGE_SIZE 4096
#define PDE_INDEX(v) (((v) >> 22) & 0x3FF) 
#define PTE_INDEX(v) (((v) >> 12) & 0x3FF)

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
#define HEAP_START 0x00400000u
#define HEAP_END 0x10000000u // 268 MiB upper bound (temp)
uint32_t heap_next_virt = HEAP_START;


extern uint32_t page_directory[];

uint32_t total_usable_memory = 0;
struct memory_entry memory_entries[MAX_MEM_ENTRIES];
static memory_block_t* kernel_block_list = NULL;
uint32_t entries = 0;

static uint8_t*  pmm_bitmap;
static uint32_t  pmm_total_pages;

static inline void pmm_set_bit(uint32_t page) {
    pmm_bitmap[page / 8] |=  (1u << (page % 8));
}

static inline void pmm_clear_bit(uint32_t page) {
    pmm_bitmap[page / 8] &= ~(1u << (page % 8));
}

static inline int pmm_test_bit(uint32_t page) {
    return (pmm_bitmap[page / 8] >> (page % 8)) & 1u;
}

void pmm_init(struct memory_entry* entries, int entry_count) {
    uint32_t highest = 0;
    for (int i = 0; i < entry_count; ++i) {
        uint32_t end = entries[i].addr + entries[i].len;
        if (end > highest) highest = end;
    }

    pmm_total_pages = (highest + PAGE_SIZE - 1) / PAGE_SIZE;

    uint32_t kernel_end_phys = (uint32_t)(uintptr_t)__kernel_end;
    uint32_t bitmap_start    = (kernel_end_phys + 0xFFF) & ~0xFFFu; // page-align
    uint32_t bitmap_bytes    = (pmm_total_pages + 7) / 8;
    uint32_t bitmap_end      = bitmap_start + bitmap_bytes;

    pmm_bitmap = (uint8_t*)(uintptr_t)bitmap_start;

    for (uint32_t i = 0; i < bitmap_bytes; ++i) {
        pmm_bitmap[i] = 0xFF;
    }

    for (int i = 0; i < entry_count; ++i) {
        uint32_t start = entries[i].addr;
        uint32_t end   = entries[i].addr + entries[i].len;

        // Skip below 1 MiB 
        if (end <= 0x00100000u) continue;
        if (start < 0x00100000u) start = 0x00100000u;

        uint32_t first_page = start / PAGE_SIZE;
        uint32_t last_page  = (end + PAGE_SIZE - 1) / PAGE_SIZE;

        for (uint32_t p = first_page; p < last_page; ++p) {
            pmm_clear_bit(p);
        }
    }

    uint32_t kstart = (uint32_t)(uintptr_t)__kernel_start;
    uint32_t kend   = (uint32_t)(uintptr_t)__kernel_end;

    uint32_t k_first_page = kstart / PAGE_SIZE;
    uint32_t k_last_page  = (kend + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t p = k_first_page; p < k_last_page; ++p) {
        pmm_set_bit(p);
    }

    uint32_t bm_first_page = bitmap_start / PAGE_SIZE;
    uint32_t bm_last_page  = (bitmap_end + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t p = bm_first_page; p < bm_last_page; ++p) {
        pmm_set_bit(p);
    }

    uint32_t pd_phys = (uint32_t)(uintptr_t)page_directory;
    uint32_t pd_page = pd_phys / PAGE_SIZE;
    pmm_set_bit(pd_page);

    extern uint32_t first_page_table[];
    uint32_t pt_phys = (uint32_t)(uintptr_t)first_page_table;
    uint32_t pt_page = pt_phys / PAGE_SIZE;
    pmm_set_bit(pt_page);

    serial_print("pmm_init: total pages=%u, bitmap at 0x%x (%u bytes)\n",
                 pmm_total_pages, bitmap_start, bitmap_bytes);
}



// helpers for sizes and flags
static inline uint32_t block_size(memory_block_t* b) {
    return b->len & ~USED_FLAG;
}
static inline bool block_used(memory_block_t* b) {
    return (b->len & USED_FLAG) != 0;
}
static inline void mark_used(memory_block_t* b) {
    b->len |= USED_FLAG;
}
static inline void mark_free(memory_block_t* b) {
    b->len &= ~USED_FLAG;
}


static void add_block(uint32_t addr, uint32_t len, memory_block_t** list) {
    if (len < sizeof(memory_block_t) + 4) return;

    memory_block_t* block = (memory_block_t*)(uintptr_t)addr;
    block->len = ALIGN4(len) & ~USED_FLAG;

    memory_block_t* prev = NULL;
    memory_block_t* cur  = *list;

    while (cur && cur < block) {
        prev = cur;
        cur  = cur->next;
    }

    block->next = cur;

    if (prev) prev->next = block;
    else      *list = block;
}

uint32_t pmm_alloc_page(void) {
    for (uint32_t page = 0; page < pmm_total_pages; ++page) {
        if (!pmm_test_bit(page)) {
            pmm_set_bit(page);
            return page * PAGE_SIZE;
        }
    }
    return 0;
}

void pmm_free_page(uint32_t phys_addr) {
    uint32_t page = phys_addr / PAGE_SIZE;
    if (page < pmm_total_pages) {
        pmm_clear_bit(page);
    }
}

void vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = PDE_INDEX(virt);
    uint32_t pt_idx = PTE_INDEX(virt);

    uint32_t pde = page_directory[pd_idx];
    uint32_t* pt;

    if (!(pde & 0x1)) {
        // No page table yet: allocate one
        uint32_t pt_phys = pmm_alloc_page();
        if (!pt_phys) {
            // handle fatal error (panic)
            serial_print("vmm_map_page: out of memory for page table\n");
            for (;;);
        }

        // Identity-mapped: we can access it at its physical address
        pt = (uint32_t*)(0xFFC00000 + (pd_idx * 0x1000));
        memset(pt, 0, PAGE_SIZE);

        page_directory[pd_idx] = (pt_phys & ~0xFFFu) | 0x3; // present writabl
    } else {
        uint32_t pt_phys = pde & ~0xFFFu;
        pt = (uint32_t*)(uintptr_t)pt_phys; // identity-mapped
    }

    pt[pt_idx] = (phys & ~0xFFFu) | (flags & 0xFFFu);

    // Invalidate this TLB entry
    asm volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

static bool heap_grow(void) {
    

    if (heap_next_virt + PAGE_SIZE > HEAP_END) {
        serial_print("heap_grow: out of virtual heap space\n");
        return false;
    }

    uint32_t phys = pmm_alloc_page();
    if (!phys) {
        serial_print("heap_grow: out of physical memory\n");
        return false;
    }
    vmm_map_page(heap_next_virt, phys, 0x3); // present | writable

    add_block(heap_next_virt, PAGE_SIZE, &kernel_block_list);

    heap_next_virt += PAGE_SIZE;
    return true;
}

void init_allocator(struct memory_entry* entries, int entry_count) {
    // 1. Initialize PMM from memory map
    pmm_init(entries, entry_count);

    // 2. Initialize heap list and virtual heap range
    kernel_block_list = NULL;
    heap_next_virt    = HEAP_START; // e.g. 0x00400000

   
}


void parse_memory_map(struct multiboot_info* mbinfo){
    
    if (!(mbinfo->flags & (1 << 6 ))) {
        serial_print("NO MEMORY MAP\n");
        return;
    }

    uint32_t mmap_end = mbinfo->mmap_addr + mbinfo->mmap_length;
    int av_entry_count = 0;

    for(uint32_t ptr = mbinfo->mmap_addr; ptr < mmap_end; ){
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)ptr;
        
        if(entry->type == MULTIBOOT_MEMORY_AVAILABLE){
            
            uint64_t addr64 = entry->addr;
            uint64_t len64  = entry->len;

            if (addr64 > 0xFFFFFFFFULL) {
                // ram is more than 4GB
                ptr += entry->size + sizeof(entry->size);
                continue;
            }

            uint64_t end64 = addr64 + len64;
            if (end64 > 0xFFFFFFFFULL) {
                len64 = 0x100000000ULL - addr64;
            }
            struct memory_entry new_entry;
            new_entry.addr = (uint32_t)addr64;
            new_entry.len  = (uint32_t)len64;

            if (new_entry.addr >= 0x100000 && new_entry.len >= 16) {
                if(av_entry_count < MAX_MEM_ENTRIES){
                    serial_print("kernel located at:%d sized %d\n", __kernel_start, (__kernel_end - __kernel_start));
                    serial_print("entry at: %d, sized: %d\n", new_entry.addr, new_entry.len);
                    memory_entries[av_entry_count++] = new_entry;
                }
            }
        }

        ptr += entry->size + sizeof(entry->size);
    }

    init_allocator(memory_entries, av_entry_count);
}



static memory_block_t* find_prev_for_size(uint32_t needed, memory_block_t** prev_out, memory_block_t* list) {
    memory_block_t* prev = NULL;
    memory_block_t* cur  = list;
    while (cur) {
     
        if (!block_used(cur) && block_size(cur) >= needed) {
            if (prev_out) *prev_out = prev;
            return cur;
        }
        prev = cur;
        cur  = cur->next;
    }
    if (prev_out) *prev_out = prev;
    return NULL;
}


void* kmalloc(uint32_t size) {
    if (!size) return NULL;

    uint32_t user_size = ALIGN4(size);
    uint32_t needed    = sizeof(memory_block_t) + user_size;
    const uint32_t min_split = sizeof(memory_block_t) + 4;

    serial_print("kmalloc request=%u needed=%u\n", size, needed);

    //multi page alloc
    if (needed > PAGE_SIZE) {
        uint32_t pages = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
        uint32_t total_size = pages * PAGE_SIZE;

        uint32_t base = heap_next_virt;

        for (uint32_t i = 0; i < pages; i++) {
            if (!heap_grow()) {
                serial_print("kmalloc: large alloc failed\n");
                return NULL;
            }
        }

        memory_block_t* block = (memory_block_t*)(uintptr_t)base;
        block->len  = total_size | USED_FLAG;
        block->next = NULL;

        serial_print("large alloc: %u pages at %x\n", pages, base);

        return (uint8_t*)block + sizeof(memory_block_t);
    }
// single page alloc
retry:
    memory_block_t* prev  = NULL;
    memory_block_t* block = find_prev_for_size(needed, &prev, kernel_block_list);

    if (!block) {
        serial_print("no block\n");
        if (!heap_grow()) {
            serial_print("kmalloc: out of memory\n");
            return NULL;
        }
        goto retry;
    }

    serial_print("block found\n");

    uint32_t bsize = block_size(block);

    if (bsize >= needed + min_split) {
        uint8_t* base = (uint8_t*)block;

        memory_block_t* new_block = (memory_block_t*)(base + needed);
        uint32_t new_size = bsize - needed;

        new_block->len  = (new_size & ~USED_FLAG);
        new_block->next = block->next;

        if (prev) prev->next = new_block;
        else      kernel_block_list = new_block;

        block->len = needed | USED_FLAG;
    } else {
        if (prev) prev->next = block->next;
        else      kernel_block_list = block->next;

        mark_used(block);
    }

    return (uint8_t*)block + sizeof(memory_block_t);
}


void kfree(void* ptr) {
    if (!ptr) return;

    memory_block_t* block = (memory_block_t*)((uint8_t*)ptr - sizeof(memory_block_t));
    mark_free(block);

    memory_block_t* prev = NULL;
    memory_block_t* cur = kernel_block_list;
    while (cur && cur < block) {
        prev = cur;
        cur = cur->next;
    }

    block->next = cur;
    if (prev) prev->next = block;
    else kernel_block_list = block;

    if (block->next && !block_used(block->next)) {
    uint8_t* block_end = (uint8_t*)block + block_size(block);
    if (block_end == (uint8_t*)block->next) {
        uint32_t merged = block_size(block) + block_size(block->next);
        block->len = (merged & ~USED_FLAG);
        block->next = block->next->next;
    }
}

    if (prev && !block_used(prev)) {
    uint8_t* prev_end = (uint8_t*)prev + block_size(prev);
    if (prev_end == (uint8_t*)block) {
        uint32_t merged = block_size(prev) + block_size(block);
        prev->len = (merged & ~USED_FLAG);
        prev->next = block->next;
    }
}
}





// TODO: add more functionality for specific scenarios
void* memcpy(void* dest, const void* src, size_t n)
{
    unsigned char* d = dest;
    const unsigned char* s = src;

    while (n--) {
        *d++ = *s++;
    }

    return dest;
}

void* memset(void *b, int c, int len)
{
  int           i;
  unsigned char *p = b;
  i = 0;
  while(len > 0)
    {
      *p = c;
      p++;
      len--;
    }
  return(b);
}

void* memmove(void* dest, const void* src, size_t n) {
    unsigned char* d = (unsigned char*)dest;
    const unsigned char* s = (const unsigned char*)src;

    if (d == s || n == 0)
        return dest;

    if (d < s) {
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = n; i != 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}