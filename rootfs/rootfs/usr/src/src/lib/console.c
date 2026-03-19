
//libs
#include "types.h"
#include "console.h"

// drivers


static putchar_func_t current_putchar_func = 0;
static clear_func_t current_clear_func = 0;
static color_func_t current_color_func = 0;
static cursor_func_t current_cursor_func = 0;
static attr_func_t current_attr_func = 0;

void setsource_console_putchar(putchar_func_t func){
    current_putchar_func = func;
}
void setsource_console_clear(clear_func_t func){
    current_clear_func = func;
}
void setsource_console_color(color_func_t func){
    current_color_func = func;
}
void setsource_console_cursor(cursor_func_t func){
    current_cursor_func = func;
}
void setsource_console_attr(attr_func_t func){
    current_attr_func = func;
}



void set_console_cursor(int x, int y){
    if(current_cursor_func){
        current_cursor_func(x,y);
    }
}

// console functions
void putchar(char c){
    if(current_putchar_func){
        current_putchar_func(c);
    }
}

void putstr(char *str){
    while(*str){
        vga_putc(*str++);
    }
}

void putint(int value){
    char buffer[12];
    int i = 0;
    bool neg = false;

    if(value == 0){
        vga_putc('0');
        return;
    }

    if(value < 0){
        neg = true;
        value = -value;
    }

    while(value > 0 && i < (int)sizeof(buffer)){
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }

    if(neg){
        vga_putc('-');
    }

    while(i-- > 0){
        vga_putc(buffer[i]);
    }
}

static void puthex(uint32_t value, int width) {
    char hex_chars[] = "0123456789abcdef";
    char buffer[9];
    int i = 0;


    do {
        buffer[i++] = hex_chars[value & 0xF];
        value >>= 4;
    } while (value && i < 8);

    while (i < width && i < 8)
        buffer[i++] = '0';

    for (int j = i - 1; j >= 0; j--)
        vga_putc(buffer[j]);
}

void put_u64(unsigned long long value) {
    char buf[21];
    int i = 0;

    if (value == 0) {
        vga_putc('0');
        return;
    }

    while (value > 0) {
        unsigned long long q = value / 10;
        unsigned long long r = value - q * 10;
        buf[i++] = '0' + (char)r;
        value = q;
    }

    while (i--)
        vga_putc(buf[i]);
}

void puthex64(unsigned long long v, int width) {
    const char *hex = "0123456789abcdef";
    char buf[16];
    int i = 0;

    for (int s = 60; s >= 0; s -= 4)
        buf[i++] = hex[(v >> s) & 0xF];

    int start = 0;
    while (start < 15 && buf[start] == '0')
        start++;

    int digits = 16 - start;
    if (width > digits) {
        for (int k = 0; k < width - digits; k++)
            vga_putc('0');
    }

    for (int k = start; k < 16; k++)
        vga_putc(buf[k]);
}

void put_i64(long long value) {
    if (value < 0) {
        vga_putc('-');
        value = -value;
    }
    put_u64((unsigned long long)value);
}

void printf(const char *fmt, ...){
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

                case 'd':
                    if (is_ll)
                        put_i64(va_arg(args, long long));
                    else
                        putint(va_arg(args, int));
                    break;

                case 'u':
                    if (is_ll)
                        put_u64(va_arg(args, unsigned long long));
                    else
                        putint(va_arg(args, unsigned int));
                    break;

                case 'x':
                    if (is_ll)
                        puthex64(va_arg(args, unsigned long long), width);
                    else
                        puthex(va_arg(args, uint32_t), width);
                    break;

                case 's':
                    putstr(va_arg(args, const char*));
                    break;

                case 'c':
                    vga_putc((char)va_arg(args, int));
                    break;

                case '%':
                    vga_putc('%');
                    break;

                default:
                    vga_putc('%');
                    vga_putc(fmt[i]);
            }

        } else {
            vga_putc(fmt[i]);
        }
    }

    va_end(args);
}

uint8_t make_attr(enum VGA_COLOR FG, enum VGA_COLOR BG, bool blink){
    if(current_attr_func){
        uint8_t attr = current_attr_func(FG, BG, blink);
        if (attr) return attr;
        return 0;
    }
}

void clear(){
    if(current_clear_func){
        current_clear_func();
    }
}

void console_color(enum VGA_COLOR FG, enum VGA_COLOR BG){
    if(current_color_func){
        current_color_func(FG, BG);
    }
}

