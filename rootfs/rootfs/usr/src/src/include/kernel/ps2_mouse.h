#ifndef PS2_MOUSE_H
#define PS2_MOUSE_H
    #include "types.h"

    void mouse_init();
    void mouse_irq_handler();
    void mouse_get_position(int* x, int* y);

    extern const int mouse_max_x;
    extern const int mouse_max_y;
#endif