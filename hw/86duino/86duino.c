 
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

unsigned char                   vtx86_86duino_flags = 0;

struct vtx86_cfg_t              vtx86_cfg = {0};
struct vtx86_gpio_port_cfg_t    vtx86_gpio_port_cfg = {0};

