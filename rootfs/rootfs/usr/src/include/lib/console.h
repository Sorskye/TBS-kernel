#include "types.h"
#include "vga-textmode.h"

typedef uint8_t (*attr_func_t)(enum VGA_COLOR FG, enum VGA_COLOR BG, bool blink);
typedef void (*putchar_func_t)(char c);
typedef void (*clear_func_t)();
typedef void (*color_func_t)(enum VGA_COLOR FG, enum VGA_COLOR BG);
typedef void (*cursor_func_t)(uint16_t x, uint16_t y);


void setsource_console_putchar(putchar_func_t func);
void setsource_console_clear(clear_func_t func);
void setsource_console_color(color_func_t func);
void setsource_console_cursor(cursor_func_t func);
void setsource_console_attr(attr_func_t func);

void set_console_cursor(int x, int y);
void putchar(char c);
void putstr(char *str);
uint8_t make_attr(enum VGA_COLOR FG, enum VGA_COLOR BG, bool blink);
void console_color(enum VGA_COLOR FG, enum VGA_COLOR BG);
void clear();

void printf(const char *fmt, ...);