 
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

#include <hw/86duino/86duino.h>

const int8_t vtx86_uart_IRQs[16] = {
    -1, 9, 3,10,
     4, 5, 7, 6,
     1,11,-1,12,
    -1,14,-1,15
};

