
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>
#include <hw/dos/doswin.h>

/* Compatible AT DMA controllers do this weird addressing on channels 4-7 where the
 * lower 16 bits are shifted right 1 bit (while not shifting the page registers).
 * This is the beautiful hack apparently that allows them to transfer WORDs at a time
 * and up to 128KB. Newer Intel DMA controllers have extended registers to select
 * 8/16/32-bit transfers and to enable/disable this shift on a per-channel basis
 * which is why we have the 16bit_ashift bitmask to track that. */
/* FIXME: So if we shift over 1 bit, what happens to the address when bit 15 is actually set? */
uint32_t d8237_read_base(unsigned char ch) {
    uint32_t r = d8237_read_base_lo16(ch);
    if (d8237_16bit_ashift & (1<<ch)) r <<= 1UL;
    r |= (uint32_t)inp(d8237_page_ioport_map[ch]) << (uint32_t)16;
    return r;
}

/* vim: set tabstop=4 softtabstop=4 shiftwidth=4 expandtab */

