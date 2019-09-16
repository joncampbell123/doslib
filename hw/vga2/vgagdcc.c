
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98
uint8_t vga2_get_dcc(void) {
    return vga2_get_dcc_inline();
}
#endif

