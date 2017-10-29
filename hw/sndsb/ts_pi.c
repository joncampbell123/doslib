
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
#include <math.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* 8237 DMA */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/sndsb/sndsb.h>
#include <hw/dos/doswin.h>

#include <hw/isapnp/isapnp.h>
#include <hw/sndsb/sb16asp.h>
#include <hw/sndsb/sndsbpnp.h>

#include "ts_pcom.h"

static char tmp[256];

int main(int argc,char **argv) {
    if (common_sb_init() != 0)
        return 1;

    assert(sb_card != NULL);

    printf("Test results will be written to TS_PI.TXT\n");

    report_fp = fopen("TS_PI.TXT","w");
    if (report_fp == NULL) return 1;

    {
        const char *ess_str;
        const char *mixer_str;

        sndsb_probe_mixer(sb_card);
        sndsb_check_for_sb16_asp(sb_card);
        sndsb_sb16_asp_ram_test(sb_card);

        if (sb_card->asp_chip_ram_ok)
            sndsb_sb16_asp_probe_chip_ram_size(sb_card);

        mixer_str = sndsb_mixer_chip_str(sb_card->mixer_chip);
        ess_str = sndsb_ess_chipset_str(sb_card->ess_chipset);

        if (sb_card->baseio != 0U)
            doubleprintf("- Base I/O base:                0x%04X\n",sb_card->baseio);
        if (sb_card->mpuio != 0U)
            doubleprintf("- MPU I/O base:                 0x%04X\n",sb_card->mpuio);
        if (sb_card->oplio != 0U)
            doubleprintf("- OPL I/O:                      0x%04X\n",sb_card->oplio);
        if (sb_card->gameio != 0U)
            doubleprintf("- Game I/O:                     0x%04X\n",sb_card->gameio);
        if (sb_card->aweio != 0U)
            doubleprintf("- AWE I/O:                      0x%04X\n",sb_card->aweio);
        if (sb_card->wssio != 0U)
            doubleprintf("- WSS I/O:                      0x%04X\n",sb_card->wssio);
        if (sb_card->dma8 >= 0)
            doubleprintf("- 8-bit DMA channel:            %u\n",sb_card->dma8);
        if (sb_card->dma16 >= 0)
            doubleprintf("- 16-bit DMA channel:           %u\n",sb_card->dma16);
        if (sb_card->irq >= 0)
            doubleprintf("- IRQ:                          %u\n",sb_card->irq);

        doubleprintf("- DSP detected:                 %u\n",sb_card->dsp_ok);
        doubleprintf("- DSP version:                  %u.%u (0x%02X 0x%02X)\n",sb_card->dsp_vmaj,sb_card->dsp_vmin,sb_card->dsp_vmaj,sb_card->dsp_vmin);
        doubleprintf("- DSP copyright string:         %s\n",sb_card->dsp_copyright);
        doubleprintf("- Mixer:                        %s\n",(sb_card->mixer_ok && mixer_str != NULL) ? mixer_str : "(none)");
        doubleprintf("- Is Gallant SC-6600:           %s\n",sb_card->is_gallant_sc6600 ? "Yes" : "No");
        doubleprintf("- ESS chipset:                  %s\n",(sb_card->ess_extensions && ess_str != NULL) ? ess_str : "(none)");

        doubleprintf("- ISA Plug & Play:              %s\n",sb_card->pnp_id != 0UL ? "Yes" : "No");
        if (sb_card->pnp_id != 0UL) {
            tmp[0] = 0;
            isa_pnp_product_id_to_str(tmp,sb_card->pnp_id);
            doubleprintf("- ISA PnP ID:                   %s\n",tmp);
            doubleprintf("- ISA PnP CSN:                  %u\n",sb_card->pnp_csn);
        }
        if (sb_card->pnp_name != NULL)
            doubleprintf("- ISA PnP string:               %s\n",sb_card->pnp_name);

        doubleprintf("- Has CSP/ASP chip:             %s\n",sb_card->has_asp_chip ? "Yes" : "No");

        if (sb_card->has_asp_chip) {
            doubleprintf("- CSP/ASP chip version ID:      0x%04X\n",sb_card->asp_chip_version_id);

            if (sb_card->asp_chip_ram_ok && sb_card->asp_chip_ram_size_probed)
                doubleprintf("- CSP/ASP RAM size:             %lu bytes\n",sb_card->asp_chip_ram_size);
            else
                doubleprintf("- CSP/ASP RAM size:             N/A (test failed)\n");
        }
    }

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

