#include "vga-textmode.h"
#include "console.h"
#include "WM.h"
#include "string.h"
#include "memory.h"
#include "kerror.h"
#include "task.h"
#include "ps2_key.h"

#define MAX_WINDOWS 10
Window windowlist[MAX_WINDOWS];
size_t windowCount = 0;
uint16_t* wm_buffer=NULL;
uint32_t wm_pid = 1;




void alloc_wm_buffer(){
    size_t size = (80 * 25 ) * sizeof(uint16_t);
    wm_buffer = malloc(size);
}

void init_wm(){
    alloc_wm_buffer();
}

uint16_t make_cell(char c, uint8_t attr) {
    return ((uint8_t)attr << 8) | (uint8_t)c;
}

void window_put_char(Window* win, uint8_t x, uint8_t y, char c, uint8_t attr) {
    if (x < 0 || y < 0 || x >= win->w || y >= win->h){
        return; }

    int index = y * win->w + x;
    
    win->framebuffer[index] = make_cell(c, attr);
}

void window_put_string(Window* win, char* str, uint8_t xpos, uint8_t ypos, enum VGA_COLOR attr){
    uint8_t localpos = xpos;
    while(*str){
        window_put_char(win, localpos, ypos, *str++, attr);
        localpos++;
    }
}

Window* get_window_from_id(uint32_t ID){
    Window* window = &windowlist[ID];
    return window;
}


void framebuffer_clear(uint16_t* framebuffer, uint8_t w, uint8_t h, enum VGA_COLOR attr) {
    uint16_t background = make_cell(' ', attr);
    for (int i = 0; i < w * h; ++i) framebuffer[i] = background;
}

// api functions

uint32_t wm_api_make_window(enum VGA_COLOR FG, enum VGA_COLOR BG, uint8_t width, uint8_t height, uint8_t x, uint8_t y, bool DrawBorder, bool canbesel, char* title){
    uint32_t arg0 =
        ((uint32_t)FG << 24) |
        ((uint32_t)BG << 16);

    uint32_t arg1 = 
        ((uint32_t)width << 24) |
        ((uint32_t)height << 16) |
        ((uint32_t)x << 8)  |
        ((uint32_t)y);
    
    message_t make_window_message = {
        WM_MAKE_WINDOW,
        arg0,
        arg1,
        (uint32_t)DrawBorder,
        (uint32_t)canbesel,
        0,
        0,
        title,
        0,
        0,
        0,

    };

    int resp = 0;
    do{resp = send_process_message(wm_pid, &make_window_message);} while (resp!=1);
    
    uint32_t ID = 0;

    while (true)
    {
        message_t* msg;
        if(receive_process_message(msg)){
            ID = msg->arg0;
            return ID;
        }
    }
    
    

    return ID;

}

int wm_api_place_text(enum VGA_COLOR FG, enum VGA_COLOR BG, uint8_t x, uint8_t y, char* str, uint32_t ID){
    uint32_t arg0 =
        ((uint32_t)FG << 24) |
        ((uint32_t)BG << 16);

    uint32_t arg1 = 
     (x << 24) | (y << 16);

    char* str_pointer = malloc(strlen((char*)str)+1);

    strcpy(str_pointer, (char*)str, strlen((char*)str)+1);

    message_t place_text_message = {
        WM_PLACE_TEXT,
        arg0,
        arg1,
        ID,
        0,
        0,
        0,
        (void*)str_pointer,
    };

    int resp = 0;
    do{resp = send_process_message(wm_pid, &place_text_message);} while (resp!=1);

    return resp;
}

int wm_api_inc_window_pos(uint8_t DeltaX, uint8_t DeltaY, bool IsNegative, uint32_t ID){
    uint32_t arg0 =
        ((uint32_t)DeltaX << 24) |
        ((uint32_t)DeltaY << 16);
    
    message_t select_window_message = {
        WM_INC_WINDOW_POS,
        arg0,
        IsNegative,
        ID
    };

    int resp = 0;
    resp = send_process_message(wm_pid, &select_window_message);

    return resp;
}

int wm_api_select_window(uint8_t ID){

    
    message_t inc_window_pos_message = {
        WM_SELECT_WINDOW,
        0,
        0,
        ID

    };

    int resp = 0;
    do{resp = send_process_message(wm_pid, &inc_window_pos_message);} while (resp!=1);

    return resp;
}

int wm_api_get_selected_window(){
     message_t get_sel_window_msg = {
        WM_GET_SELECTED_WINDOW,
    };

    int resp = 0;
    do{resp = send_process_message(wm_pid, &get_sel_window_msg);} while (resp!=1);

    uint32_t selected = 0;
    void handle_process_message(message_t *msg){
        selected = msg->arg0;
    }

    size_t tries = 0;
    while(tries < 50){
        message_t msg;
        if(receive_process_message(&msg)){
            handle_process_message(&msg);
        }
        tries++;
    }
   

    return selected;
    
}

void wm_api_remove_window(uint32_t window){
    message_t remove_window_msg = {
        WM_REMOVE_WINDOW,
        window,
    };

    int resp = 0;
    do{resp = send_process_message(wm_pid, &remove_window_msg);} while (resp!=1);
}

void draw_window(Window* window, uint8_t FG, uint8_t BG, bool border){

    int cursor_X = 0;
    int cursor_Y = 0;
    char* title = window->title;
    
    int WindowWidth = window->w;
    int WindowHeight = window->h;
    int WinPosX = window->posx;
    int WinPosY = window->posy;
    uint16_t* framebuffer = window->framebuffer;

    uint8_t ATTRIBUTES = make_attr(FG, BG, false);
    framebuffer_clear(framebuffer, WindowWidth, WindowHeight, ATTRIBUTES);


    if(border){
        // top border
        for(int x=0; x<WindowWidth; x++){
            window_put_char(window, x, 0, 0xCD, ATTRIBUTES);
        }
        // bottom border
        for(int x=0; x<WindowWidth; x++){
            window_put_char(window, x, WindowHeight-1, 0xCD, ATTRIBUTES);
        }

        // left border
        for(int y=0; y<WindowHeight; y++){
            window_put_char(window, 0, y, 0xBA, ATTRIBUTES);
        }
            // right border
        for(int y=0; y<WindowHeight; y++){
        
        window_put_char(window, WindowWidth-1, y, 0xBA, ATTRIBUTES);
        }


        // topleft corner
        window_put_char(window, 0, 0, 0xC9, ATTRIBUTES);
        // bottomleft corner
        window_put_char(window, 0, WindowHeight-1, 0xC8, ATTRIBUTES);
        // topright corner
        window_put_char(window, WindowWidth-1, 0, 0xBB, ATTRIBUTES);
        // bottomright corner
        window_put_char(window, WindowWidth-1, WindowHeight-1, 0xBC, ATTRIBUTES);

        //titleholder left
        window_put_char(window, 1, 0, 0xB5, ATTRIBUTES);

        cursor_X = 2; cursor_Y = 0;
        
    
        uint8_t TitleLength = strlen(title);
        
        uint8_t title_attr = make_attr(FG, BG, true);
        if(TitleLength<=WindowWidth-3){
            
            window_put_string(window, title, cursor_X, cursor_Y, title_attr);
            cursor_X+=TitleLength;
            
        }else{
            for(int x=0; x<WindowWidth-4; x++){
                window_put_char(window, cursor_X, cursor_Y, title[x], title_attr);
                cursor_X++;
            }
        }
        
        if(TitleLength>WindowWidth-2){
            cursor_X = WinPosX+WindowWidth-4; cursor_Y = 0;
            window_put_string(window, "..", cursor_X, cursor_Y, title_attr);
        }

        window_put_char(window, cursor_X, 0, 0xC6, ATTRIBUTES);
    }
}

void select_window(int index) {
    Window* win = get_window_from_id(index);
    if (index >= 0 && index < windowCount && win->canbesel == true) {
        windowlist[index].z = windowCount;

        for (int i = 0; i < windowCount; i++) {
            if (i != index && windowlist[i].z >= windowlist[index].z) {
                windowlist[i].z--;
            }
        }
    }
}

void remove_window(int index){
    for (int i = index; i < windowCount - 1; ++i) {
        windowlist[i] = windowlist[i + 1];
    }
    windowCount--;
    return;
}

Window* get_window_from_index(uint32_t index){
    Window* window = &windowlist[index];
    return window;
}


uint32_t make_window( enum VGA_COLOR FG, enum VGA_COLOR BG, uint8_t w, uint8_t h, uint8_t x, uint8_t y, bool border, bool canbesel, char *title){

    if (windowCount >= 10){return 0;}
    
    size_t fb_size = (w * h)* sizeof(uint16_t);
    uint16_t* framebuffer = malloc(fb_size);
    if(!framebuffer)return 0;
    framebuffer_clear(framebuffer, w, h, (FG,BG, false));
    
    Window* newWindow = &windowlist[windowCount];
    newWindow->w = w;
    newWindow->h = h;
    newWindow->posx = x;
    newWindow->posy = y;
    newWindow->framebuffer = framebuffer;
    newWindow->fg = FG;
    newWindow->bg = BG;
    newWindow->border = border;
    newWindow->canbesel = canbesel;
    strcpy(newWindow->title, title, strlen(title));
    newWindow->title[sizeof(newWindow->title) - 1] = '\0';
    
    select_window(windowCount);
    draw_window(newWindow, FG,BG, border);

    windowCount++;

    return windowCount-1;
}

void blit_window_to_wm(Window *win, uint16_t *wm_buffer) {
    if (!win || !win->framebuffer || !wm_buffer) return;
    
    int w = win->w;
    int h = win->h;
    int base_x = win->posx;
    int base_y = win->posy;


    if (w <= 0 || h <= 0) return;

    for (int wy = 0; wy < h; ++wy) {
        int sy = base_y + wy;
        if (sy < 0 || sy >= GRID_HEIGHT) continue; // vertical clip

        for (int wx = 0; wx < w; ++wx) {
            int sx = base_x + wx;
            if (sx < 0 || sx >= GRID_WITDH) continue; // horizontal clip

            size_t src_idx = (size_t)wy * (size_t)w + (size_t)wx;
            uint16_t cell = win->framebuffer[src_idx];

            if (cell == 0x0000) continue;

            wm_buffer[(size_t)sy * GRID_WITDH + (size_t)sx] = cell;
        }
    }
}

//TODO: compositor opnieuw schrijven om alleen benodigde veranderingen te tekenen
void draw_gui_framebuffer(){
    
    if(windowCount==0){return;}
    framebuffer_clear(wm_buffer, GRID_WITDH, GRID_HEIGHT, 0x0F);
    
    for (int i = 0; i < windowCount; i++) {
        for (int j = 0; j < windowCount - i - 1; j++) {
            if (windowlist[j].z > windowlist[j + 1].z) {
                Window temp = windowlist[j];
                windowlist[j] = windowlist[j + 1];
                windowlist[j + 1] = temp;
            }
        }
    }
    
    for (int z = 0; z < windowCount; z++) {
        Window* CurWindow = &windowlist[z];
        blit_window_to_wm(CurWindow, wm_buffer);

    }

    vga_push_framebuffer(wm_buffer);

    

    
}

int32_t selector = 0;
int32_t incrementor = 1;
void wm_handle_keyboard_input(message_t* msg){
 
    uint8_t scanline = (msg->arg0 >> 24) & 0xFF;
    char key = translate_scancode(scanline);
    
    uint32_t window = windowCount-1;
  
    switch (scanline){
    case 0x4D:
        wm_api_inc_window_pos(1,0,false,window);
        break;
    case 0x4B:
        wm_api_inc_window_pos(1,0,true,window);
        break;
    case 0x48:
        wm_api_inc_window_pos(0,1,true,window);
        break;
    case 0x50:
        wm_api_inc_window_pos(0,1,false,window);
        break;
    case 0x1D:
        
       
        select_window(1);
    default:
        break;
    }
    return;
}

void handle_process_message(message_t* msg){

    if(msg->type == PS2_NEW_INPUT){
        wm_handle_keyboard_input(msg);
        return;
    }
    
    uint32_t type = msg->type;
    uint32_t arg0 = msg->arg0;
    uint32_t arg1 = msg->arg1;
    uint32_t arg2 = msg->arg2;
    uint32_t arg3 = msg->arg3;
    uint32_t arg4 = msg->arg4;
    uint32_t arg5 = msg->arg5;
    void*    data0 = msg->data0;
    void*    data1 = msg->data1;
    void*    data2 = msg->data2;
    uint32_t PID_SENDER = msg->PID_SENDER;
   
    uint32_t ID;
    Window* window;
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;
    uint8_t FG;
    uint8_t BG;
    
    switch (type)
    {
    case WM_MAKE_WINDOW:
        // arg0 = vga FG / BG (uint8_t)
        // arg1 = w, h, x, y (uint8_t)
        // arg2 = Window pointer
        // arg3 = border (bool)
        // data0* = title
        // data1* = window pointer
        FG = (arg0 >> 24) & 0xFF;
        BG = (arg0 >> 16) & 0xFF;

        w = (arg1 >> 24) & 0xFF;
        h = (arg1 >> 16) & 0xFF;
        x = (arg1 >> 8) & 0xFF;
        y = (arg1) & 0xFF;

        bool border = (bool)arg2;
        bool canbesel = (bool)arg3;
        char* title = data0;
        ID = make_window(FG, BG, w, h, x, y, border, canbesel, title);  


        message_t return_ID_msg = {
            0, // type
            ID,
            0,
            0,
            0,
            0,
            0,
            0,

        };

        uint32_t resp;
        do
        {
           resp = send_process_message(PID_SENDER, &return_ID_msg);
        } while (resp != 1);
        
        
        break;
        case WM_PLACE_TEXT:


        // arg0 = vga FG / BG (uint8_T)
        // arg1 = x, y (uint8_t)
        // arg2 = Window* window
        // data0* = title
        FG = (arg0 >> 24) & 0xFF;
        BG = (arg0 >> 16) & 0xFF;
        uint8_t attr = make_attr(FG, BG, false);

        x = (arg1 >> 24) & 0xFF;
        y = (arg1 >> 16) & 0xFF;
 
        ID = arg2;

        char* str = (char*)data0;
        window = get_window_from_id(ID);
        
        window_put_string(window, str, x, y, attr);
        free(data0);
        break;
    case WM_INC_WINDOW_POS:
        uint8_t DeltaX = (arg0 >> 24) & 0xFF;
        uint8_t DeltaY = (arg0 >> 16) & 0xFF;
        bool IsNegative = (bool)arg1;
        ID = arg2;

        window = get_window_from_id(ID);
        switch (IsNegative)
        {
        case true:
            window->posx -= DeltaX;
            window->posy -= DeltaY;
            break;
        case false:
            window->posx += DeltaX;
            window->posy += DeltaY;
            break;
        }

        if (window->posx+DeltaX > GRID_WITDH-window->w){
            window->posx = GRID_WITDH-window->w;
        }
        if (window->posx - DeltaX > window->posx){
            window->posx = 0;
        }
        if(window->posy+DeltaY > GRID_HEIGHT-window->h ){
            window->posy = GRID_HEIGHT-window->h;
        }
        if(window->posy - DeltaY > window->posy ){
            window->posy = 0;
        }

        break;

    case WM_SELECT_WINDOW:
        ID = arg2;
        select_window(ID);
        break;
    case WM_GET_SELECTED_WINDOW:

        uint32_t selwin = windowCount;
        message_t give_sel_win_msg = {
            WM_GIVE_SELECTED_WINDOW,
            selwin,
        };
        resp = send_process_message(PID_SENDER, &give_sel_win_msg);


    case WM_REMOVE_WINDOW:
        uint32_t window = arg0;
        remove_window(window);
    default:
        break;
    }
    return;
}

void wm_loop(){
    task_t* wmtask = PID_to_task(wm_pid);
    keyboard_subscribe(wmtask);
    while (True)
    {
        
        message_t msg;
        if(receive_process_message(&msg)){
            handle_process_message(&msg);
        }

        draw_gui_framebuffer();
    }

}