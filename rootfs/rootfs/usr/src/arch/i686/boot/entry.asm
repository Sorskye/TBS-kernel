section .multiboot
align 4
    MULTIBOOT_MAGIC      equ 0x1BADB002
    MULTIBOOT_FLAGS      equ (1 << 0) | (1 << 1)    ; no memory map, no video mode, simple setup
    MULTIBOOT_CHECKSUM   equ -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

    dd MULTIBOOT_MAGIC
    dd MULTIBOOT_FLAGS
    dd MULTIBOOT_CHECKSUM

section .text
align 4
bits 32
extern kernel_main
extern no_sse

global gdt_flush
global page_directory
global first_page_table

gdt_flush:
    
    mov eax, [esp + 4]
    lgdt [eax]

    jmp 0x08:flush_cs_reload

flush_cs_reload:
    mov ax, 0x10 ; 0x10 = gdt index 2 (kernel data)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global _start
_start:
    mov esp, stack_top
    push ebx        ; multibooot memory map pointer
    push eax        ; magic number

    mov eax, 1
    cpuid
    test edx, (1<<25)
    jz no_sse

    ; Clear page directory
mov edi, page_directory
mov ecx, 1024
xor eax, eax
rep stosd


;; create tables - identity map 1GB of physical memory
mov esi, first_page_table
xor ebx, ebx             
mov ecx, 256
setup_tables:

    push ecx

    ; Fill one page table
    mov edi, esi
    mov ecx, 1024

.fill_pt:
    mov eax, ebx
    or eax, 0x3
    mov [edi], eax

    add ebx, 0x1000
    add edi, 4
    loop .fill_pt

    ; Add to page directory
    mov eax, esi
    or eax, 0x3

    mov edx, page_directory
    mov ecx, 256
    sub ecx, [esp]
    mov [edx + ecx*4], eax

    add esi, 4096

    pop ecx
    loop setup_tables

    ;SSE is available
   
    call kernel_main

.hang:
    hlt
    jmp .hang

section .bss
align 4096
page_directory:
    resb 4096

align 4096
first_page_table:
    resb 256 * 4096  ; 256 page tables = 1 GB of physical memory identity-mapped

stack_bottom: resb 4096
stack_top: