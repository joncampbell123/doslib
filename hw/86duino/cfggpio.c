 
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

int read_vtx86_gpio_pin_config(void) {
    unsigned int i;
    uint32_t tmp;

    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED))
        return 0;

    vtx86_gpio_port_cfg.gpio_dec_enable = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x00);
    for (i=0;i < 10;i++) {
        tmp = inpd(vtx86_cfg.gpio_portconfig_base_io + 0x04 + (i * 4u));
        vtx86_gpio_port_cfg.gpio_pingroup[i].data_io = (uint16_t)(tmp & 0xFFFFul);
        vtx86_gpio_port_cfg.gpio_pingroup[i].dir_io  = (uint16_t)(tmp >> 16ul);
    }

    return 1;
}

