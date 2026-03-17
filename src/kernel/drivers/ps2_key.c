#include "types.h"
#include "task.h"
#include "memory.h"
#include "ps2_key.h"
#include "IDT.h"
#include "io.h"
#include "kerror.h"

#include "serial.h"
#include "tty.h"

static bool shift = false;


static const char scancode_to_ascii[128] = {
    0,   27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n', 0,
    'a','s','d','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x',
    'c','v','b','n','m',',','.','/', 0,'*', 0,' ', 0,
};

static const char scancode_to_ascii_shift[128] = {
    0,   27, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0,
    'A','S','D','F','G','H','J','K','L',':','"','~', 0,'|','Z','X',
    'C','V','B','N','M','<','>','?', 0,'*', 0,' ', 0,
};


// task context




// irq context 

char translate_scancode(uint8_t sc)
{
    // Key release
    if (sc & 0x80)
    {
        uint8_t code = sc & 0x7F;

        if (code == SC_LSHIFT_PRESS || code == SC_RSHIFT_PRESS)
            shift = false;

        return 0;
    }

    // Key press
    if (sc == SC_LSHIFT_PRESS || sc == SC_RSHIFT_PRESS)
    {
        shift = true;
        return 0;
    }

    if (sc > 127)
        return 0;

    if (shift)
        return scancode_to_ascii_shift[sc];
    else
        return scancode_to_ascii[sc];
}



static inline void kb_ring_handle_input(uint8_t sc) {

    char c = translate_scancode(sc);

    if(c == 0){return;}

    int next_head = (active_tty->raw_input_head + 1) %TTY_INPUT_BUFF_SIZE;

    if (next_head != active_tty->raw_input_tail) {
      
        active_tty->raw_input_buff[active_tty->raw_input_head] = c;
        active_tty->raw_input_head = next_head;
        wake_task(active_tty->task_worker_wait);
    }
    return;
}

void keyboard_irq(){
    uint8_t scancode = inb(0x60);
    kb_ring_handle_input(scancode);
    return;
}

