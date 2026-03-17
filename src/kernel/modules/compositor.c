#include "types.h"
#include "vga-textmode.h"
#include "serial.h"
#include "sleep.h"
#include "task.h"
#include "tty.h"
#include "memory.h"
#include "spinlock.h"


enum  VGA_COLOR PRI_BG_COLOR = VGA_BLUE;
enum VGA_COLOR SEC_BG_COLOR = VGA_LIGHT_GREY;
enum VGA_COLOR PRI_FG_COLOR = VGA_YELLOW;
enum VGA_COLOR SEC_FG_COLOR = VGA_BLACK;

#define SCROLL_X0 1
#define SCROLL_X1 78
#define SCROLL_Y0 1
#define SCROLL_Y1 22

uint16_t cursor_x = SCROLL_X0;
uint16_t cursor_y = SCROLL_Y0;


void draw_console_border(int x, int width){
    int y = 0;

        char line_flat        = 0xC4; // ─
        char line_flat_double = 0xCD; // ═
        char line_vert        = 0xB3; // │
        char corner_tl        = 0xD5; // ╒
        char corner_tr        = 0xB8; // ╕ 
        char corner_bl        = 0xC0; // └
        char corner_br        = 0xD9; // ┘
        
    vga_set_color(PRI_FG_COLOR, PRI_BG_COLOR);
    // corner top left
    vga_putc(corner_tl, x, y);

    // corner top right
    vga_putc(corner_tr, x+width-1, y);

    // corner bottom left

    vga_putc(corner_bl, x, y+23);

    // corner bottom right
    vga_putc(corner_br, x+width-1, y+23);



    // top bar
    for (int bar_x = x+1; bar_x<width -1 ; bar_x++){
        vga_putc(line_flat_double, bar_x, y);
    }

    // bottom bar
    for (int bar_x = x+1; bar_x<width -1 ; bar_x++){
        vga_putc(line_flat, bar_x, y+23);
    }

    // left bar
    for (int bar_y = y+1; bar_y<23 ; bar_y++){
        vga_putc(line_vert, x, bar_y);
    }

    // right bar
    for (int bar_y = y+1; bar_y<23 ; bar_y++){
        vga_putc(line_vert, x+width-1, bar_y);
    }
}

void draw_background(){

    // primary
    vga_set_color(PRI_FG_COLOR, PRI_BG_COLOR);
    for (int x = 0; x <= 80; x++){
        // < 24 because bottom row = menu bar
        for (int y = 0; y < 24; y++){
            vga_putc(' ', x, y);
        }
    }

    // secondary
    vga_set_color(SEC_FG_COLOR, SEC_BG_COLOR);
    for (int x = 0; x < 80; x++){
        vga_putc(' ', x, 24);
    }

}

void compositor_draw_loop(){
    while(true){
        vga_refresh();
        sleep_ms(16);
    }
}



void compositor_init(){

    draw_background();
    draw_console_border(0,80);
    vga_refresh();
  //  task_t* compositor_loop = create_ktask((void*)compositor_draw_loop, 0);
}

char read_tty_output(tty_t* tty)
{   
    
    while (tty->output_head == tty->output_tail)
    {   
        block_task(current_task);
        __asm__ __volatile__("int $32");
    }
    spinlock_acquire(&tty->output_lock);
    char c = tty->output_buff[tty->output_tail];
    tty->output_tail = (tty->output_tail + 1) % TTY_OUTPUT_BUFF_SIZE;
    spinlock_release(&tty->output_lock);

    return c;
}

void compositor_clear_screen() {
    
    uint16_t* vga = vga_framebuffer;
    volatile uint8_t VGA_ATTR = PRI_FG_COLOR | PRI_BG_COLOR << 4;

    const int WIDTH  = SCROLL_X1 - SCROLL_X0 + 1;   // 78 columns
    const int HEIGHT = SCROLL_Y1 - SCROLL_Y0 + 2;   // 22 rows

    for (int x = SCROLL_X0; x < WIDTH; x++){
        for(int y = SCROLL_Y0; y < HEIGHT; y++){
            const size_t index = y * GRID_WIDTH + x;
            vga_framebuffer[index] = ((uint8_t)VGA_ATTR << 8) | (uint8_t)' ';
        }
    }
    
    cursor_x = SCROLL_X0;
    cursor_y = SCROLL_Y0-1;

    vga_refresh();
    return;
}



void compositor_scroll() {
    
    uint16_t* vga = vga_framebuffer;
    volatile uint8_t VGA_ATTR = PRI_FG_COLOR | PRI_BG_COLOR << 4;
    const int WIDTH  = SCROLL_X1 - SCROLL_X0 + 1;   // 78 columns
    const int HEIGHT = SCROLL_Y1 - SCROLL_Y0 + 1;   // 22 rows

    for (int y = 0; y < HEIGHT - 1; y++) {
        int dst_row = SCROLL_Y0 + y;
        int src_row = SCROLL_Y0 + y + 1;    

        uint16_t* dst = vga + dst_row * GRID_WIDTH + SCROLL_X0;
        uint16_t* src = vga + src_row * GRID_WIDTH + SCROLL_X0;

        memmove(dst, src, WIDTH * sizeof(uint16_t));
    }

    uint16_t* row = vga + SCROLL_Y1 * GRID_WIDTH + SCROLL_X0;

    for (int x = 0; x < WIDTH-1; x++) {
        row[x] = (uint16_t)' ' | (VGA_ATTR << 8);
    }
    
    return;
}



void compositor_main(){
    
    compositor_init();
    int height = 22;
    int width = 78;
    vga_set_cursor(cursor_x, cursor_y);
    while(1){
        
        char c = read_tty_output(active_tty);
        
        if (c == '\b') {
            if (cursor_x > SCROLL_X0) {
                cursor_x--;
                vga_putc(' ', cursor_x, cursor_y);
            } else if (cursor_y > SCROLL_Y0) {
                cursor_y--;
                cursor_x = SCROLL_X1;
                vga_putc(' ', cursor_x, cursor_y);
            }
        } else if (c == '\n' || cursor_x >= SCROLL_X1) {
            cursor_x = SCROLL_X0;
            if (cursor_y < SCROLL_Y1) {
                cursor_y++;
            } else {
                compositor_scroll();
                cursor_y = SCROLL_Y1;
            }
        } else {
            vga_putc(c, cursor_x, cursor_y);
            cursor_x++;
        }
        
        vga_set_cursor(cursor_x, cursor_y);
        vga_refresh();
    }
}
