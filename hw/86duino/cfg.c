 
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

int read_vtx86_config(void) {
    if (!(vtx86_86duino_flags & VTX86_86DUINO_FLAG_DETECTED))
        return 0;

    vtx86_cfg.uart_config_base_io = vtx86_sb_readw(0x60);
    if (vtx86_cfg.uart_config_base_io & 1u)
        vtx86_cfg.uart_config_base_io &= ~1u;
    else
        vtx86_cfg.uart_config_base_io  = 0;

    vtx86_cfg.gpio_portconfig_base_io = vtx86_sb_readw(0x62);
    if (vtx86_cfg.gpio_portconfig_base_io & 1u)
        vtx86_cfg.gpio_portconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_portconfig_base_io  = 0;

    vtx86_cfg.gpio_intconfig_base_io = vtx86_sb_readw(0x66);
    if (vtx86_cfg.gpio_intconfig_base_io & 1u)
        vtx86_cfg.gpio_intconfig_base_io &= ~1u;
    else
        vtx86_cfg.gpio_intconfig_base_io  = 0;

    vtx86_cfg.crossbar_config_base_io = vtx86_sb_readw(0x64);
    if (vtx86_cfg.crossbar_config_base_io & 1u)
        vtx86_cfg.crossbar_config_base_io &= ~1u;
    else
        vtx86_cfg.crossbar_config_base_io  = 0;

    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_SB1) {
        // NTS: Bit 0 is not set and has no effect on my 86Duino --J.C.
        vtx86_cfg.adc_config_base_io = vtx86_sb1_readw(0xE0) & (~1u);
    }
    else {
        vtx86_cfg.adc_config_base_io = 0;
    }

    if (vtx86_86duino_flags & VTX86_86DUINO_FLAG_MC) {
        vtx86_cfg.pwm_config_base_io = vtx86_mc_readw(0x10);
        if (vtx86_cfg.pwm_config_base_io & 1u)
            vtx86_cfg.pwm_config_base_io &= ~1u;
        else
            vtx86_cfg.pwm_config_base_io  = 0;
    }
    else {
        vtx86_cfg.pwm_config_base_io = 0;
    }

    return 1;
}

