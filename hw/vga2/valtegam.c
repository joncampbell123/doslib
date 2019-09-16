
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

#ifndef TARGET_PC98
unsigned char vga2_alt_ega_switches_assume(void) {
    return vga2_alt_ega_switches_assume_inline();
}
#endif

