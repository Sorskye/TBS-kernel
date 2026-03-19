
#include "IDT.h"
#include "kerror.h"
#include "symbols.h"
#include "task.h"
#include "serial.h"
#include "vga-textmode.h"
#include "string.h"
#include "lpcspeak.h"

int panic_cursor_x = 0;
int panic_cursor_y = 0;
#define WIDTH 80
#define HEIGHT 25

void halt(){
    while (true){
        asm volatile ("hlt");
    }
}

const char* lookup_symbol(uint32_t addr) {
    const char* result = "unknown";
    uint32_t best = 0;

    for (int i = 0; i < kernel_symbol_count; i++) {
        uint32_t sym = kernel_symbols[i].addr;

        if (sym <= addr && sym >= best) {
            best = sym;
            result = kernel_symbols[i].name;
        }
    }

    return result;
}

void clear_screen(){
    vga_set_color(VGA_WHITE, VGA_RED);
    for(int x=0; x<WIDTH; x++){
        for(int y=0; y<HEIGHT; y++){
            vga_putc(' ',x,y);
        }
    }
    vga_refresh();
}
void printp(const char *fmt, ...) {

    char out[1024];
    
    size_t out_i = 0;

    va_list args;
    va_start(args, fmt);

    for (size_t i = 0; fmt[i] != '\0'; ++i) {

        if (fmt[i] == '%') {
            i++;

            int width = 0;
            if (fmt[i] == '0') {
                i++;
                while (fmt[i] >= '0' && fmt[i] <= '9') {
                    width = width * 10 + (fmt[i] - '0');
                    i++;
                }
            }

            int is_ll = 0;
            if (fmt[i] == 'l' && fmt[i+1] == 'l') {
                is_ll = 1;
                i += 2;
            }

            switch (fmt[i]) {

                case 'd': {
                    if (is_ll)
                        out_i += i64_to_str(va_arg(args, long long), out + out_i);
                    else
                        out_i += int_to_str(va_arg(args, int), out + out_i);
                    break;
                }

                case 'u': {
                    if (is_ll)
                        out_i += u64_to_str(va_arg(args, unsigned long long), out + out_i);
                    else
                        out_i += uint_to_str(va_arg(args, unsigned int), out + out_i);
                    break;
                }

                case 'x': {
                    if (is_ll)
                        out_i += hex64_to_str(va_arg(args, unsigned long long), out + out_i, width);
                    else
                        out_i += hex32_to_str(va_arg(args, uint32_t), out + out_i, width);
                    break;
                }

                case 's': {
                    const char* s = va_arg(args, const char*);
                    while (*s) out[out_i++] = *s++;
                    break;
                }

                case 'c': {
                    out[out_i++] = (char)va_arg(args, int);
                    break;
                }

                case '%': {
                    out[out_i++] = '%';
                    break;
                }

                default: {
                    out[out_i++] = '%';
                    out[out_i++] = fmt[i];
                }
            }

        } else {
            out[out_i++] = fmt[i];
        }

        if (out_i >= sizeof(out) - 1)
            break;
    }

    out[out_i] = '\0';
    va_end(args);

    serial_print(out);

    for (int i = 0; out[i] != '\0'; i++){
        char c = out[i];
        if (c == '\n'){
            panic_cursor_x = 0;
            panic_cursor_y++;
        }else{
            vga_putc(out[i], panic_cursor_x, panic_cursor_y);
            panic_cursor_x++;
        }
       
    }

    vga_refresh();
    return;
}


void print_stack_trace(panic_registers *regs) {
    uint32_t *ebp = (uint32_t*)regs->ebp;
    uint32_t eip = regs->eip;

    if(regs == 0){
        printp("no register information\n");
    }

    printp("Stack trace:\n");
    printp(" -> %08x : %s\n", eip, lookup_symbol(eip));

    int frame_count = 0;
    while (ebp && frame_count < 32) { // limit frames to avoid infinite loop
        uint32_t ret = ebp[0];

        // sanity check return address

        printp(" -> %08x : %s\n", ret, lookup_symbol(ret));

        uint32_t *next_ebp = (uint32_t*)ebp[0];

        // sanity check next EBP
        if (next_ebp <= ebp) 
            break;  // prevent loops or stack underflow

        ebp = next_ebp;
        frame_count++;
    }
}

void print_eflags(uint32_t eflags)
{
    printp("EFLAGS = 0x%08x\n", eflags);

    printp("CF = %d | ",  (eflags >> 0) & 1);
    printp("PF = %d | ",  (eflags >> 2) & 1);
    printp("AF = %d |",  (eflags >> 4) & 1);
    printp("ZF = %d | ",  (eflags >> 6) & 1);
    printp("SF = %d | ",  (eflags >> 7) & 1);
    printp("TF = %d\n",  (eflags >> 8) & 1);
    printp("IF = %d | ",  (eflags >> 9) & 1);
    printp("DF = %d | ",  (eflags >> 10) & 1);
    printp("OF = %d |",  (eflags >> 11) & 1);

    printp("IOPL = %d | ",  (eflags >> 12) & 3);

    printp("NT = %d | ",  (eflags >> 14) & 1);
    printp("RF = %d\n",  (eflags >> 16) & 1);
    printp("VM = %d | ",  (eflags >> 17) & 1);
    printp("AC = %d | ",  (eflags >> 18) & 1);
    printp("VIF = %d \n",  (eflags >> 19) & 1);
    printp("VIP = %d | ",  (eflags >> 20) & 1);
    printp("ID = %d\n\n",  (eflags >> 21) & 1);
}

void panic(char* panic_msg, panic_registers *regs){
    __asm__ __volatile__("cli");
    serial_print("PANIC\n");
    
    speaker_sound_panic();


    clear_screen();
    __asm__ __volatile__("cli");
    char* msg = interruptMessages[regs->intNum];
    if(regs->eip != 0){
        printp("KERNEL PANIC! FAULT_ID >> %s\n",msg);
    }else{
        printp("KERNEL PANIC! FAULT_ID >> %s\n",panic_msg);
        printp("No further information\n");
    }
    printp("PID TID >> %d, %d\n",0,current_task->tid);
  
   

    if(in_interrupt>1){
        printp("_WHILE_IRQ_%s\n",current_irq_name);
        printp("IRQ_NEST: %d\n",in_interrupt);
    }

    if(regs->eip != 0){
    printp("CS: 0x%08x | ",regs->cs);
    printp("DS: 0x%08x\n",regs->ds);
    printp("ES: 0x%08x | ",regs->es);
    printp("GS: 0x%08x\n",regs->gs);

    printp("EAX: 0x%08x | ",regs->eax);
    printp("EBX: 0x%08x\n",regs->ebx);
    printp("ECX: 0x%08x | ",regs->ecx);
    printp("EDX: 0x%08x\n",regs->edx);
    printp("ESI: 0x%08x | ",regs->esi);
    printp("EDI: 0x%08x\n",regs->edi);
    printp("ESP: 0x%08x | ",regs->esp);
    printp("EBP: 0x%08x\n",regs->ebp);
    
   
    print_eflags(regs->eflags);

    printp("FAULT AT EIP: %08x > %s\n",regs->eip, lookup_symbol(regs->eip));
   
   // print_stack_trace(regs);
    }
    
    vga_refresh();
    __asm__ __volatile__("cli");
    halt();
}


Char *interruptMessages[] = {

    "DIVISION_FAULT",       //0 
    "DEBUG",                  //1
    "NON_MASKABLE_INT", //2
    "BREAKPOINT",             //3
    "INTO_DETECTED_OVERFLOW", //4
    "OUT_OF_BOUND",          //5
    "INVALID_OPTCODE",         //6
    "NO_COPROCESSOR",         //7

    "DOUBLE_FAULT",                //8
    "COPROCESSOR_SEGMENT_OVERRUN", //9
    "BAD_TSS",                     //10
    "SEGMENT_NOT_PRESENT",         //11
    "STACK_FAULT",                 //12
    "GENERAL_PROTECTION",    //13
    "PAGE_FAULT", //14
    "UNKNOWN_INTERRUPT", //15

    "COPROCESSOR_FAULT", //16
    "ALIGNMENT_CHECK", //17
    "MACHINE_CHECK", //18
    "Reserved", //19
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};




Char *irqMessages[] = {
    "SYSTEM_TIMER",       //0
    "KEYBOARD_PS/2",                  //1
    "INT_CONTROLLER", //2
    "SERIAL_CONTROLLER_2",             //3
    "SERIAL_CONTROLLER_1", //4
    "PARRALLEL_PORT_3_OR_ISA_SOUND",          //5
    "FLOPPY_CONTROLLER",         //6
    "PARRALLEL_PORT_1",         //7

    "RTC",                //8
    "ACPI", //9
    "PERIPHERAL_1",                     //10
    "PERIPHERAL_2",         //11
    "MOUSE_PS/2",                 //12
    "CO_PROCESSOR_OR_FLOATING_POINT_UNIT",    //13
    "ATA_PRIMARY",
    "ATA_SECONDARY",
};
