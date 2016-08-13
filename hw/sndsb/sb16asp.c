
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <direct.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/sndsb/sndsb.h>
#include <hw/sndsb/sb16asp.h>

/* SB16 ASP set codec parameter */
int sndsb_sb16asp_set_codec_parameter(struct sndsb_ctx *cx,uint8_t param,uint8_t value) {
    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x05) < 0) // set codec parameter <value> <param>
        return -1;
    if (sndsb_write_dsp(cx,value) < 0)
        return -1;
    if (sndsb_write_dsp(cx,param) < 0)
        return -1;

    return 0;
}

/* read back parameter index of last set codec param command */
int sndsb_sb16asp_read_last_codec_parameter(struct sndsb_ctx *cx) {
    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x03) < 0) // ?? this is what the Linux kernel does... don't know if it's needed
        return -1;

    return sndsb_read_dsp(cx);
}

int sndsb_sb16asp_set_mode_register(struct sndsb_ctx *cx,uint8_t val) {
    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x04) < 0) // set mode register
        return -1;
    if (sndsb_write_dsp(cx,val) < 0)
        return -1;

    return 0;
}

int sndsb_sb16asp_set_register(struct sndsb_ctx *cx,uint8_t reg,uint8_t val) {
    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x0E) < 0) // set register
        return -1;
    if (sndsb_write_dsp(cx,reg) < 0)
        return -1;
    if (sndsb_write_dsp(cx,val) < 0)
        return -1;

    return 0;
}

int sndsb_sb16asp_get_register(struct sndsb_ctx *cx,uint8_t reg) {
    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x0F) < 0) // get register
        return -1;
    if (sndsb_write_dsp(cx,reg) < 0)
        return -1;

    return sndsb_read_dsp(cx);
}

int sndsb_sb16asp_get_version(struct sndsb_ctx *cx) {
    int val;

    if (!cx->has_asp_chip)
        return -1;

    if (sndsb_write_dsp(cx,0x08) < 0)
        return -1;
    if (sndsb_write_dsp(cx,0x03) < 0)
        return -1;

    val = sndsb_read_dsp(cx);
    if (val < 0x10 || val > 0x1F)
        return -1;

    cx->asp_chip_version_id = (uint8_t)val;
    return 0;
}

/* check for Sound Blaster 16 ASP/CSP chip */
int sndsb_check_for_sb16_asp(struct sndsb_ctx *cx) {
    unsigned int i;
    int t1,t2;

    if (cx == NULL) return 0;
    if (cx->probed_asp_chip) return cx->has_asp_chip; // don't probe twice
    if (cx->dsp_vmaj != 4) return 0; // must be Sound Blaster 16
    if (cx->is_gallant_sc6600) return 0; // must not be Gallant SC-6600 / Reveal SC-400

    cx->has_asp_chip = 1;
    cx->probed_asp_chip = 1;
    cx->asp_chip_ram_ok = 0;
    cx->asp_chip_version_id = 0;

    if (sndsb_sb16asp_set_codec_parameter(cx,0x00,0x00) < 0)
        goto fail_need_reset;

    // NTS: We use the Linux kernel sequence (mode=0xFC, read/write 0x83 which in that mode becomes some sort of scratch register)
    //      Creative Labs DIAGNOSE.EXE instead uses mode == 0xFA and mode == 0xF9 to read/write the first 4 bytes of
    //      the ASP chip's internal RAM. Specifically, it likes to read 4 bytes, invert the bits, write the new bytes,
    //      read them back, validate the inverted bits wrote, then invert the bits again and test the original bytes
    //      come back. We don't do that here.
    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0) // 0xFC == ??
        goto fail_need_reset;

    if ((t1=sndsb_sb16asp_get_register(cx,0x83)) < 0)
        goto fail_need_reset;

    for (i=0;i < 4;i++) {
        if (sndsb_sb16asp_set_register(cx,0x83,(uint8_t)(~t1)))
            goto fail_need_reset;
        if ((t2=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_need_reset;
        if (sndsb_sb16asp_set_register(cx,0x83,(uint8_t)t1))
            goto fail_need_reset;
        if (t1 != (t2 ^ 0xFF))
            goto fail;
        if ((t2=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_need_reset;
        if (t1 != t2)
            goto fail;
    }

    // test is done
    if (sndsb_sb16asp_set_mode_register(cx,0x00) < 0) // 0x00 == ??
        goto fail_need_reset;

    if (sndsb_sb16asp_get_version(cx) < 0)
        goto fail_need_reset;

    // reasonable assumption is 2048 bytes.
    // caller can ask us to probe deeper to check that.
    // the probing might not be 100% compatible and it inevitably has to overwrite
    // some bytes in RAM so unless it matters it's not recommended you carry out the test.
    cx->asp_chip_ram_size = 2048;

	sndsb_reset_dsp(cx);
    return 1;
fail_need_reset:
	sndsb_reset_dsp(cx);
fail:
    if (sndsb_sb16asp_set_mode_register(cx,0x00) < 0) // 0x00 == ??
        goto fail_need_reset;
	sndsb_reset_dsp(cx);
    cx->has_asp_chip = 0;
    return 0;
}

/* ASP RAM test.
 * This is the same test procedure used by Creative's DIAGNOSE.EXE utility.
 * Read the first 4 bytes, inverting all the bits and writing them back in place.
 * Read them back and confirm our inverted bytes made it safely. Then invert the bits
 * again, write them back, and read them back to confirm they made it safely. */
int sndsb_sb16_asp_ram_test(struct sndsb_ctx *cx) {
    unsigned char tmp[4];
    unsigned int i;
    int v;

    if (cx == NULL) return 0;
    if (!cx->has_asp_chip) return 0;

    cx->asp_chip_ram_ok = 0;

    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0) // memory access mode, reset memory index
        goto fail_reset;
    if (sndsb_sb16asp_set_mode_register(cx,0xFA) < 0) // memory access mode, increment on write
        goto fail_reset;

    for (i=0;i < 4;i++) {
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_reset;
        v ^= 0xFF;
        if (sndsb_sb16asp_set_register(cx,0x83,v) < 0)
            goto fail_reset;

        tmp[i] = (unsigned char)v; // remember what we wrote
    }

    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0) // memory access mode, reset memory index
        goto fail_reset;
    if (sndsb_sb16asp_set_mode_register(cx,0xF9) < 0) // memory access mode, increment on read
        goto fail_reset;

    for (i=0;i < 4;i++) {
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_reset;
        if (v != tmp[i])
            goto fail_reset;
    }

    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0) // memory access mode, reset memory index
        goto fail_reset;
    if (sndsb_sb16asp_set_mode_register(cx,0xFA) < 0) // memory access mode, increment on write
        goto fail_reset;

    for (i=0;i < 4;i++) {
        v = tmp[i] ^ 0xFF; // invert again
        if (sndsb_sb16asp_set_register(cx,0x83,v) < 0)
            goto fail_reset;

        tmp[i] = (unsigned char)v; // remember what we wrote
    }

    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0) // memory access mode, reset memory index
        goto fail_reset;
    if (sndsb_sb16asp_set_mode_register(cx,0xF9) < 0) // memory access mode, increment on read
        goto fail_reset;

    for (i=0;i < 4;i++) {
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_reset;
        if (v != tmp[i])
            goto fail_reset;
    }

    cx->asp_chip_ram_ok = 1; // it passes!
    return cx->asp_chip_ram_ok;
fail_reset:
    sndsb_reset_dsp(cx);
    sndsb_sb16asp_set_mode_register(cx,0x00);
    sndsb_reset_dsp(cx);
    return 0;
}

