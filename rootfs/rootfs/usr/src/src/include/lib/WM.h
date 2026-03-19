#include "vga-textmode.h"

typedef struct {
    uint8_t posx, posy, w, h;
    char title[80];
    uint16_t* framebuffer;
    int z;
    bool border;
    bool canbesel;
    enum VGA_COLOR fg;
    enum VGA_COLOR bg;
} Window;

#define WM_MAKE_WINDOW 0
#define WM_PLACE_TEXT 1
#define WM_INC_WINDOW_POS 2
#define WM_SELECT_WINDOW 3
#define WM_GET_SELECTED_WINDOW 4
#define WM_GIVE_SELECTED_WINDOW 5
#define WM_REMOVE_WINDOW 6



void draw_core(enum VGA_COLOR FG, enum VGA_COLOR BG);

void draw_topbar_text(char *str);
void draw_gui_framebuffer();
void init_wm();
void wm_loop();

// api
uint32_t wm_api_make_window(enum VGA_COLOR FG, enum VGA_COLOR BG, uint8_t width, uint8_t height, uint8_t x, uint8_t y, bool DrawBorder, bool canbesel, char* title);
int wm_api_place_text(enum VGA_COLOR FG, enum VGA_COLOR BG, uint8_t x, uint8_t y, char* str, uint32_t ID);
int wm_api_inc_window_pos(uint8_t DeltaX, uint8_t DeltaY,bool IsNegative, uint32_t ID);
int wm_api_select_window(uint8_t ID);
void wm_api_remove_window(uint32_t window);
int wm_api_get_selected_window();