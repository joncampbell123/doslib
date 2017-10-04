
#include <hw/sndsb/sndsb.h>
#include <hw/sndsb/sb16asp.h>

int sndsb_sb16_asp_probe_chip_ram_size(struct sndsb_ctx * const cx) {
    unsigned char pattern[32],old[32];
    unsigned long count=0;
    unsigned int i;
    int v;

    if (!cx->has_asp_chip) return 0;
    if (cx->asp_chip_ram_size_probed) return 1;
    cx->asp_chip_ram_size_probed = 1;

    /* generate pattern not likely to conflict with anything else in chip RAM */
    for (i=0;i < 32;i++) pattern[i] = (unsigned char)rand();

    /* read and save first 32 bytes */
    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0)
        goto fail;
    if (sndsb_sb16asp_set_mode_register(cx,0xF9) < 0)
        goto fail;
    for (i=0;i < 32;i++) {
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail;
        old[i] = (unsigned char)v;
    }

    /* write pattern */
    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0)
        goto fail;
    if (sndsb_sb16asp_set_mode_register(cx,0xFA) < 0)
        goto fail;
    for (i=0;i < 32;i++) {
        if (sndsb_sb16asp_set_register(cx,0x83,pattern[i]) < 0)
            goto fail;
    }

    /* read and confirm our pattern */
    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0)
        goto fail;
    if (sndsb_sb16asp_set_mode_register(cx,0xF9) < 0)
        goto fail;
    for (i=0;i < 32;i++) {
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail;
        if (v != pattern[i])
            goto fail;
    }

    /* then continue reading until we see our pattern again (or 32KB of reading has been done). the internal RAM isn't really all that large. */
    i = 0; // matching index
    while (count < (32UL << 10UL)) {
        count++;
        if ((v=sndsb_sb16asp_get_register(cx,0x83)) < 0)
            goto fail;

        if (v == pattern[i]) {
            if ((++i) == 32) {
                cx->asp_chip_ram_size = (uint16_t)count;
                break;
            }
        }
        else {
            i = 0; // not a match, reset
        }
    }

    /* write old data back */
    if (sndsb_sb16asp_set_mode_register(cx,0xFC) < 0)
        goto fail;
    if (sndsb_sb16asp_set_mode_register(cx,0xFA) < 0)
        goto fail;
    for (i=0;i < 32;i++) {
        if (sndsb_sb16asp_set_register(cx,0x83,old[i]) < 0)
            goto fail;
    }

    return 1;
fail:
    sndsb_reset_dsp(cx);
    sndsb_sb16asp_set_mode_register(cx,0x00);
    sndsb_reset_dsp(cx);
    return 0;
}

