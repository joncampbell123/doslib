
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
    cx->asp_chip_version_id = 0;

    if (sndsb_sb16asp_set_codec_parameter(cx,0x00,0x00) < 0)
        goto fail_need_reset;

    // NTS: The Linux kernel has the correct sequence (mode=0xFC, then test register).
    //      Creative's software seems to do it wrong, not sure how it worked (mode=0xFC then mode=0xFA, then test register).
    //      On my SB16 ASP, setting mode=0xFA then writing the register allows ONE write, then further writes have no effect (and then this test fails).
    //      While at the same time, setting mode=0xFC allows the register to change at will.
    // TODO: What does Creative's DIAGNOSE.EXE accomplish by setting mode == 0xFA?
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

#if 0 // NOPE. This test is not reliable.
    // The next test is not done by the Linux kernel, but Creative's DIAGNOSE.EXE carries out this test:
    // Setting mode == 0xF9, then reading (not writing) register 0x83, causes all bits in the register to toggle.
    // This behavior is confirmed to happen on real hardware (SB16 non-PnP with ASP).
    if (sndsb_sb16asp_set_mode_register(cx,0xF9) < 0) // 0xF9 == ??
        goto fail_need_reset;

    // read value.
    // Noted on real hardware, is that the read back will return what we last wrote from the above test.
    if ((t1=sndsb_sb16asp_get_register(cx,0x83)) < 0)
        goto fail_need_reset;

    // then read the value again and expect all bits to toggle. else the bits fail.
    // NTS: Actually on real hardware, register 0x83 will only toggle twice, then stop.
    //      So if we set mode == 0xF9, if the last value we wrote was 0xFF, then we'll
    //      read back 0xFF (what we wrote), then 0x00, then 0xFF, and then because the
    //      bits stop toggling, we'll read 0xFF afterwards.
    t2 = t1 ^ 0xFF;
    for (i=0;i < 2;i++) {
        if ((t1=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail_need_reset;

        if (t1 != t2) goto fail_need_reset;
        t2 = t1 ^ 0xFF;
    }
#endif

    // test is done
    if (sndsb_sb16asp_set_mode_register(cx,0x00) < 0) // 0x00 == ??
        goto fail_need_reset;

    if (sndsb_sb16asp_get_version(cx) < 0)
        goto fail_need_reset;

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

