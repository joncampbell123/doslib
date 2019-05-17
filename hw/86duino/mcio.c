 
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

void vtx86_mc_outp(const int mc, const uint32_t idx, const uint32_t val) {
    SAVE_CPUFLAGS( _cli() ) {
        outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
        outpd(vtx86_cfg.pwm_config_base_io + (unsigned)idx, val);
    } RESTORE_CPUFLAGS();
}

uint32_t vtx86_mc_inp(const int mc, const uint32_t idx) {
    uint32_t tmp;

    SAVE_CPUFLAGS( _cli() ) {
        outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
        tmp = inpd(vtx86_cfg.pwm_config_base_io + (unsigned)idx);
    } RESTORE_CPUFLAGS();

    return tmp;
}

void vtx86_mc_outpb(const int mc, const uint32_t idx, const unsigned char val) {
    SAVE_CPUFLAGS( _cli() ) {
        outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
        outp(vtx86_cfg.pwm_config_base_io + (unsigned)idx, val);
    } RESTORE_CPUFLAGS();
}

unsigned char vtx86_mc_inpb(const int mc, const uint32_t idx) {
    unsigned char tmp;

    SAVE_CPUFLAGS( _cli() ) {
        outpd(vtx86_cfg.pwm_config_base_io + 0xd0, 1UL<<(mc+1));  // paging to corresponding reg window 
        tmp = inp(vtx86_cfg.pwm_config_base_io + (unsigned)idx);
    } RESTORE_CPUFLAGS();

    return tmp;
}

