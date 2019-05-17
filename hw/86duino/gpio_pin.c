 
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

#include <hw/cpu/cpu.h>
#include <hw/86duino/86duino.h>

void vtx86_pinMode(const uint8_t pin, const uint8_t mode) {
#define VTX86_PINMODE_TRI_STATE         (0x00)
#define VTX86_PINMODE_PULL_UP           (0x01)
#define VTX86_PINMODE_PULL_DOWN         (0x02)

    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    uint16_t dbport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dbport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].dir_io;
    if (dbport == 0) return;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    SAVE_CPUFLAGS( _cli() ) {
        if (mode == VTX86_INPUT)
        {
            outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_TRI_STATE);
            outp(dbport, inp(dbport) & (~cbio_bitmask));
        }
        else if(mode == VTX86_INPUT_PULLDOWN)
        {
            outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_PULL_DOWN);
            outp(dbport, inp(dbport) & (~cbio_bitmask));
        }
        else if (mode == VTX86_INPUT_PULLUP)
        {
            outp(vtx86_cfg.crossbar_config_base_io + 0x30 + crossbar_bit, VTX86_PINMODE_PULL_UP);
            outp(dbport, inp(dbport) & (~cbio_bitmask));
        }
        else {
            outp(dbport, inp(dbport) |   cbio_bitmask );
        }
    } RESTORE_CPUFLAGS();

#undef VTX86_PINMODE_TRI_STATE
#undef VTX86_PINMODE_PULL_UP
#undef VTX86_PINMODE_PULL_DOWN
}

