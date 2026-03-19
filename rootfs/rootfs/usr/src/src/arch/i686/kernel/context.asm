section .text
global context_switch
global scheduler_switch_now

extern current_task
extern scheduler_choose_next
extern in_interrupt


; void context_switch(uint32_t **old_sp_ptr, uint32_t *new_sp)
context_switch:

    mov eax, [esp + 4]       ; eax = old_sp_ptr
    mov edx, [esp + 8]       ; edx = new_sp

    pusha                    
    mov [eax], esp
    mov esp, edx
    popa

    ret

scheduler_switch_now:
    cli
    inc dword [in_interrupt]

    mov eax, [current_task]
    mov [eax], esp

    call scheduler_choose_next

    mov eax, [current_task]
    mov esp, [eax]
    
    dec dword [in_interrupt]
    sti
    ret