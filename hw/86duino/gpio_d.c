 
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

void vtx86_digitalWrite(const uint8_t pin,const uint8_t val) {
    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    uint16_t dport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].data_io;
    if (dport == 0) return;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    if (crossbar_bit >= 32u)
        outp(vtx86_cfg.crossbar_config_base_io + 0x80 + (crossbar_bit / 8u), 0x01);
    else {
        outp(vtx86_cfg.crossbar_config_base_io + 0x90 + crossbar_bit, 0x01);
        vtx86_Close_Pwm(pin);
    }

    SAVE_CPUFLAGS( _cli() ) {
        if (val == VTX86_LOW)
            outp(dport, inp(dport) & (~cbio_bitmask));
        else
            outp(dport, inp(dport) |   cbio_bitmask );
    } RESTORE_CPUFLAGS();
}

unsigned int vtx86_digitalRead(const uint8_t pin) {
    unsigned char crossbar_bit;
    unsigned char cbio_bitmask;
    uint16_t dport;

    if (pin >= sizeof(vtx86_gpio_to_crossbar_pin_map)) return ~0u;

    crossbar_bit = vtx86_gpio_to_crossbar_pin_map[pin];
    dport = vtx86_gpio_port_cfg.gpio_pingroup[crossbar_bit / 8u].data_io;
    if (dport == 0) return ~0u;

    cbio_bitmask = 1u << (crossbar_bit % 8u);

    if (crossbar_bit >= 32u)
        outp(vtx86_cfg.crossbar_config_base_io + 0x80 + (crossbar_bit / 8u), 0x01);
    else
        outp(vtx86_cfg.crossbar_config_base_io + 0x90 + crossbar_bit, 0x01);

    return (inp(dport) & cbio_bitmask) ? VTX86_HIGH : VTX86_LOW;
}

