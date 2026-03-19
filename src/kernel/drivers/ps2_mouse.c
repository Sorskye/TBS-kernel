#include "types.h"
#include "ps2_mouse.h"
#include "io.h"
#include "spinlock.h"
#include "serial.h"

static spinlock_t mouse_lock = {0};
const int mouse_max_x = 1000;
const int mouse_max_y = 1000;
static int mouse_x = mouse_max_x / 2;
static int mouse_y = mouse_max_y / 2;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];

void mouse_get_position(int* x, int* y) {
    spinlock_acquire(&mouse_lock);
    *x = mouse_x;
    *y = mouse_y;
    spinlock_release(&mouse_lock);
}


void mouse_wait(uint8_t type) {
    // type = 0 → wait for data
    // type = 1 → wait for input buffer to be clear
    uint32_t timeout = 100000;
    if (type == 0) {
        while (timeout--) {
            if (inb(0x64) & 1) return;
        }
    } else {
        while (timeout--) {
            if (!(inb(0x64) & 2)) return;
        }
    }
}

void mouse_write(uint8_t value) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, value);
}

uint8_t mouse_read() {
    mouse_wait(0);
    return inb(0x60);
}

void mouse_init() {
    serial_print("Initializing PS/2 mouse...\n");
    uint8_t status;

    mouse_wait(1);
    outb(0x64, 0xA8);

    mouse_wait(1);
    outb(0x64, 0x20);
    mouse_wait(0);
    status = inb(0x60);

    // enable mouse IRQ
    status |= 2;

    mouse_wait(1);
    outb(0x64, 0x60); 
    mouse_wait(1);
    outb(0x60, status);

    // 3. Reset mouse to defaults
    mouse_write(0xF6);
    mouse_read();

    // 4. Enable data reporting
    mouse_write(0xF4);
    mouse_read();
}

// interrupt context


void mouse_irq_handler() {
    uint8_t status = inb(0x64);
    if (!(status & 0x20))
        return; // not mouse data

    uint8_t data = inb(0x60);

    switch (mouse_cycle) {
    case 0:
        // First byte must have bit 3 set
        if (!(data & 0x08))
            return; // wait for a valid start byte
        mouse_bytes[0] = data;
        mouse_cycle = 1;
        return;

    case 1:
        mouse_bytes[1] = data;
        mouse_cycle = 2;
        return;

    case 2:
        mouse_bytes[2] = data;
        mouse_cycle = 0;
        break;
    }

    int dx = (int8_t)mouse_bytes[1];
    int dy = (int8_t)mouse_bytes[2];

    

    mouse_x += dx;
    mouse_y -= dy; // invert Y

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_x > mouse_max_x) mouse_x = mouse_max_x;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_y > mouse_max_y) mouse_y = mouse_max_y;

    
}