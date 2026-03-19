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



void pmm_init(struct memory_entry* entries, int entry_count, struct multiboot_info* mbinfo)
{
    // 1) Find highest physical address and find modules
    uint32_t highest = 0;
    uint32_t module_start_phys = 0xFFFFFFFFu;  // Track earliest module
    
    for (int i = 0; i < entry_count; ++i) {
        uint32_t end = entries[i].addr + entries[i].len;
        if (end > highest) highest = end;
    }
    
    if (mbinfo && mbinfo->mods_count > 0) {
        multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbinfo->mods_addr;
        for (uint32_t i = 0; i < mbinfo->mods_count; ++i) {
            if (mods[i].mod_start < module_start_phys) {
                module_start_phys = mods[i].mod_start;
            }
        }
    }

    pmm_total_pages = (highest + PAGE_SIZE - 1) / PAGE_SIZE;

    // 2) Place bitmap BETWEEN page tables and modules to avoid corruption
    // Find end of kernel structures (kernel + page tables)
    extern uint32_t first_page_table[];
    uint32_t pt_phys = (uint32_t)(uintptr_t)first_page_table;
    uint32_t pt_end = pt_phys + (256 * PAGE_SIZE);  // 256 page tables
    
    // Align bitmap to page boundary, placed right after page tables
    uint32_t bitmap_start = (pt_end + 0xFFF) & ~0xFFFu;
    
    // Safety check: ensure bitmap doesn't overlap with modules
    uint32_t bitmap_bytes = (pmm_total_pages + 7) / 8;
    uint32_t bitmap_end = bitmap_start + bitmap_bytes;
    
    if (bitmap_end > module_start_phys) {
        serial_print("WARNING: bitmap (0x%x-0x%x) would overlap module (starts 0x%x)!\n",
                     bitmap_start, bitmap_end, module_start_phys);
        // Fallback: place bitmap after modules (will need special handling to map it)
        highest = module_start_phys + PAGE_SIZE * 100;  // assume modules don't exceed 400KB
        bitmap_start = (highest + 0xFFF) & ~0xFFFu;
    }

    pmm_bitmap = (uint8_t*)(uintptr_t)bitmap_start;

    // 3) Default: mark all pages used
    serial_print("Initializing bitmap: range 0x%x-0x%x (%u bytes)\n", 
                 bitmap_start, bitmap_end, bitmap_bytes);
    for (uint32_t i = 0; i < bitmap_bytes; ++i) {
        pmm_bitmap[i] = 0xFF;
    }

    // 4) Mark usable RAM (above 1 MiB) as free
    for (int i = 0; i < entry_count; ++i) {
        uint32_t start = entries[i].addr;
        uint32_t end   = entries[i].addr + entries[i].len;

        // Skip below 1 MiB
        if (end <= 0x00100000u) continue;
        if (start < 0x00100000u) start = 0x00100000u;

        uint32_t first_page = start / PAGE_SIZE;
        uint32_t last_page  = (end + PAGE_SIZE - 1) / PAGE_SIZE;

        for (uint32_t p = first_page; p < last_page; ++p) {
            pmm_clear_bit(p); // mark free
        }
    }

    // 5) Reserve kernel image
    uint32_t kstart = (uint32_t)(uintptr_t)__kernel_start;
    uint32_t kend   = (uint32_t)(uintptr_t)__kernel_end;

    uint32_t k_first_page = kstart / PAGE_SIZE;
    uint32_t k_last_page  = (kend + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t p = k_first_page; p < k_last_page; ++p) {
        pmm_set_bit(p);
    }

    // 6) Reserve bitmap itself
    uint32_t bm_first_page = bitmap_start / PAGE_SIZE;
    uint32_t bm_last_page  = (bitmap_end + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint32_t p = bm_first_page; p < bm_last_page; ++p) {
        pmm_set_bit(p);
    }
    serial_print("Reserved bitmap: pages %u-%u (phys 0x%x - 0x%x)\n", 
                 bm_first_page, bm_last_page - 1, bitmap_start, bitmap_end - 1);

    // 7) Reserve page directory and first page tables (ALL 256 of them!)
    uint32_t pd_phys = (uint32_t)(uintptr_t)page_directory;
    uint32_t pd_page = pd_phys / PAGE_SIZE;
    pmm_set_bit(pd_page);
    serial_print("Reserved page directory: phys=0x%x page=%u\n", pd_phys, pd_page);

    uint32_t pt_first_page = pt_phys / PAGE_SIZE;
    serial_print("first_page_table at phys=0x%x (page %u)\n", pt_phys, pt_first_page);
    
    // Reserve all 256 page tables (256 pages total = 1 MB)
    for (uint32_t p = pt_first_page; p < pt_first_page + 256; ++p) {
        pmm_set_bit(p);
    }
    serial_print("Reserved page tables: pages %u-%u (phys 0x%x - 0x%x)\n", 
                 pt_first_page, pt_first_page + 255, 
                 pt_phys, pt_end - 1);

    // 8) Reserve all multiboot modules (e.g. your filesystem image)
    if (mbinfo && mbinfo->mods_count > 0) {
        multiboot_module_t* mods = (multiboot_module_t*)(uintptr_t)mbinfo->mods_addr;

        for (uint32_t i = 0; i < mbinfo->mods_count; ++i) {
            uint32_t start = mods[i].mod_start;
            uint32_t end   = mods[i].mod_end;

            uint32_t first_page = start / PAGE_SIZE;
            uint32_t last_page  = (end + PAGE_SIZE - 1) / PAGE_SIZE;

            serial_print("Reserving module %u: phys 0x%x-0x%x (pages %u-%u)\n", 
                         i, start, end, first_page, last_page - 1);

            for (uint32_t p = first_page; p < last_page; ++p) {
                pmm_set_bit(p); // mark module frames as used
            }
        }
    }

    serial_print("pmm_init complete: total pages=%u, bitmap at 0x%x (%u bytes)\n",
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

bool vmm_check_identity_page(uint32_t addr) {
    uint32_t pd_idx = (addr >> 22) & 0x3FF;
    uint32_t pt_idx = (addr >> 12) & 0x3FF;

    uint32_t pde = page_directory[pd_idx];
    if (!(pde & 0x1)) {
        return false;
    }

    uint32_t pt_phys = pde & ~0xFFFu;
    uint32_t* pt = (uint32_t*)(uintptr_t)pt_phys; // assumes PTs are identity-mapped

    uint32_t pte = pt[pt_idx];
    if (!(pte & 0x1)) {
        return false;
    }

    uint32_t phys = (pte & ~0xFFFu) | (addr & 0xFFFu);
    if (phys != addr) {
        return false;
    }

    return true;
}

bool vmm_check_identity_range(uint32_t start, uint32_t end) {
    for (uint32_t a = start; a < end; a += PAGE_SIZE) {
        if (!vmm_check_identity_page(a)) {
            return false;
        }
    }
    return true;
}

void debug_module_pte(uint32_t phys_addr, struct multiboot_info* mbinfo) {
    extern uint32_t page_directory[];
    
    uint32_t pde_idx = (phys_addr >> 22) & 0x3FF;
    uint32_t pte_idx = (phys_addr >> 12) & 0x3FF;
    
    serial_print("=== DEBUG MODULE PTE ===\n");
    serial_print("Looking up phys address 0x%x:\n", phys_addr);
    serial_print("  PDE index: %u (0x%x)\n", pde_idx, pde_idx);
    serial_print("  PTE index: %u (0x%x)\n", pte_idx, pte_idx);
    
    uint32_t pde = page_directory[pde_idx];
    serial_print("  PDE[%u] = 0x%x\n", pde_idx, pde);
    
    if (!(pde & 0x1)) {
        serial_print("  ERROR: PDE not present!\n");
        return;
    }
    
    uint32_t pt_phys = pde & ~0xFFFu;
    serial_print("  Page table physical address: 0x%x\n", pt_phys);
    
    // Access page table (should be identity mapped)
    uint32_t* pt = (uint32_t*)(uintptr_t)pt_phys;
    uint32_t pte = pt[pte_idx];
    serial_print("  PTE[%u] = 0x%x\n", pte_idx, pte);
    
    if (!(pte & 0x1)) {
        serial_print("  ERROR: PTE not present!\n");
    } else {
        uint32_t mapped_phys = pte & ~0xFFFu;
        serial_print("  Maps to physical: 0x%x\n", mapped_phys);
        if (mapped_phys != (phys_addr & ~0xFFFu)) {
            serial_print("  WARNING: Physical address mismatch! Expected 0x%x\n", phys_addr & ~0xFFFu);
        }
    }
    
    // Read a test byte to see if access works
    volatile uint8_t* test = (volatile uint8_t*)(uintptr_t)phys_addr;
    uint8_t byte = *test;
    serial_print("  Read byte at 0x%x: 0x%02x\n", phys_addr, byte);
    
    serial_print("=== END DEBUG ===\n");
}

// Verify that page table pages haven't been allocated elsewhere
void verify_page_table_integrity(void) {
    extern uint32_t first_page_table[];
    uint32_t pt_phys = (uint32_t)(uintptr_t)first_page_table;
    uint32_t pt_first_page = pt_phys / PAGE_SIZE;
    
    serial_print("=== PAGE TABLE INTEGRITY CHECK ===\n");
    serial_print("Page table region: pages %u-%u\n", pt_first_page, pt_first_page + 255);
    
    for (uint32_t p = pt_first_page; p < pt_first_page + 256; ++p) {
        if (!pmm_test_bit(p)) {
            serial_print("ERROR: Page table page %u (0x%x) is marked FREE!\n", p, p * PAGE_SIZE);
        }
    }
    serial_print("=== END CHECK ===\n");
}

void init_allocator(struct memory_entry* entries, int entry_count, struct multiboot_info* mbinfo) {
    // 1. Initialize PMM from memory map
    pmm_init(entries, entry_count, mbinfo);

    // 2. Initialize heap list and virtual heap range
    // Note: All physical memory is now identity-mapped at boot (see entry.asm: 1GB mappings)
    kernel_block_list = NULL;
    heap_next_virt    = HEAP_START; // e.g. 0x00400000
}


void parse_memory_map(struct multiboot_info* mbinfo){
    
    if (!(mbinfo->flags & (1 << 6 ))) {
        serial_print("NO MEMORY MAP FLAG SET\n");
        return;
    }

    uint32_t mmap_end = mbinfo->mmap_addr + mbinfo->mmap_length;
    int av_entry_count = 0;

    serial_print("=== GRUB MEMORY MAP ===\n");
    serial_print("mmap_addr=0x%x, mmap_length=0x%x, mmap_end=0x%x\n", 
                 mbinfo->mmap_addr, mbinfo->mmap_length, mmap_end);
    
    for(uint32_t ptr = mbinfo->mmap_addr; ptr < mmap_end; ){
        struct multiboot_mmap_entry* entry = (struct multiboot_mmap_entry*)ptr;
        
        // The entry size field tells us how big this entry is (excluding the size field itself)
        uint32_t entry_size = entry->size;
        
        // Print entry with proper 64-bit handling
        uint32_t addr_low = (uint32_t)(entry->addr & 0xFFFFFFFF);
        uint32_t addr_high = (uint32_t)((entry->addr >> 32) & 0xFFFFFFFF);
        uint32_t len_low = (uint32_t)(entry->len & 0xFFFFFFFF);
        uint32_t len_high = (uint32_t)((entry->len >> 32) & 0xFFFFFFFF);
        
        serial_print("Entry at 0x%x (size=%u): addr=0x%x_%x len=0x%x_%x type=%u\n", 
                     ptr, entry_size, addr_high, addr_low, len_high, len_low, entry->type);
        
        if(entry->type == MULTIBOOT_MEMORY_AVAILABLE){
            uint64_t addr64 = entry->addr;
            uint64_t len64  = entry->len;

            if (addr64 > 0xFFFFFFFFULL) {
                serial_print("  Skipping: address > 4GB\n");
                ptr += entry_size + sizeof(entry_size);
                continue;
            }

            uint64_t end64 = addr64 + len64;
            if (end64 > 0xFFFFFFFFULL) {
                len64 = 0x100000000ULL - addr64;
                serial_print("  Clamping: end to 4GB\n");
            }
            struct memory_entry new_entry;
            new_entry.addr = (uint32_t)addr64;
            new_entry.len  = (uint32_t)len64;

            if (new_entry.addr >= 0x100000 && new_entry.len >= 16) {
                if(av_entry_count < MAX_MEM_ENTRIES){
                    serial_print("  adding entry: 0x%x - 0x%x (size: 0x%x)\n", 
                                 new_entry.addr, new_entry.addr + new_entry.len, new_entry.len);
                    memory_entries[av_entry_count++] = new_entry;
                }
            }
        }

        ptr += entry_size + sizeof(entry_size);
    }
    serial_print("=== END MEMORY MAP (parsed %d entries) ===\n", av_entry_count);

    init_allocator(memory_entries, av_entry_count, mbinfo);
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

    //multi page alloc
    if (needed > PAGE_SIZE) {
        uint32_t pages = (needed + PAGE_SIZE - 1) / PAGE_SIZE;
        uint32_t total_size = pages * PAGE_SIZE;

        uint32_t base = heap_next_virt;

        for (uint32_t i = 0; i < pages; i++) {
            if (!heap_grow()) {
                return NULL;
            }
        }

        memory_block_t* block = (memory_block_t*)(uintptr_t)base;
        block->len  = total_size | USED_FLAG;
        block->next = NULL;

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