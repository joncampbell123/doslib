
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

void (*vga2_set_alpha_width)(const unsigned int w,const unsigned int str);

