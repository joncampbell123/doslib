 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/86duino/86duino.h>

const uint8_t vtx86_gpio_to_crossbar_pin_map[45/*PINS*/] =
   {11, 10, 39, 23, 37, 20, 19, 35,
    33, 17, 28, 27, 32, 25, 12, 13,
    14, 15, 24, 26, 29, 47, 46, 45,
    44, 43, 42, 41, 40,  1,  3,  4,
    31,  0,  2,  5, 22, 30,  6, 38,
    36, 34, 16, 18, 21};

