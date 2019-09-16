
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

unsigned char vga2_alt_ega_switches(void) {
    return vga2_alt_ega_switches_inline();
}

