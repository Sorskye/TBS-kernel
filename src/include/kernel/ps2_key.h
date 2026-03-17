
#pragma once
#ifndef PS2_KEY
#define PS2_KEY

#include "task.h"
#define PS2_NEW_INPUT 60

#define SC_LSHIFT_PRESS   0x2A
#define SC_RSHIFT_PRESS   0x36
#define SC_LSHIFT_RELEASE 0xAA
#define SC_RSHIFT_RELEASE 0xB6

void keyboard_irq();
char translate_scancode(uint8_t sc);


#endif