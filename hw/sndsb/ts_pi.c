
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
#include <hw/sndsb/sndsbpnp.h>

#include "ts_pcom.h"

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

        mixer_str = sndsb_mixer_chip_str(sb_card->mixer_chip);
        ess_str = sndsb_ess_chipset_str(sb_card->ess_chipset);

        if (sb_card->baseio != 0U)
            printf("- Base I/O base:                0x%04X\n",sb_card->baseio);
        if (sb_card->mpuio != 0U)
            printf("- MPU I/O base:                 0x%04X\n",sb_card->mpuio);
        if (sb_card->oplio != 0U)
            printf("- OPL I/O:                      0x%04X\n",sb_card->oplio);
        if (sb_card->gameio != 0U)
            printf("- Game I/O:                     0x%04X\n",sb_card->gameio);
        if (sb_card->aweio != 0U)
            printf("- AWE I/O:                      0x%04X\n",sb_card->aweio);
        if (sb_card->wssio != 0U)
            printf("- WSS I/O:                      0x%04X\n",sb_card->wssio);
        if (sb_card->dma8 >= 0)
            printf("- 8-bit DMA channel:            %u\n",sb_card->dma8);
        if (sb_card->dma16 >= 0)
            printf("- 16-bit DMA channel:           %u\n",sb_card->dma16);
        if (sb_card->irq >= 0)
            printf("- IRQ:                          %u\n",sb_card->irq);

        printf("- DSP detected:                 %u\n",sb_card->dsp_ok);
        printf("- DSP version:                  %u.%u (0x%02X 0x%02X)\n",sb_card->dsp_vmaj,sb_card->dsp_vmin,sb_card->dsp_vmaj,sb_card->dsp_vmin);
        printf("- DSP copyright string:         %s\n",sb_card->dsp_copyright);
        printf("- Mixer:                        %s\n",(sb_card->mixer_ok && mixer_str != NULL) ? mixer_str : "(none)");
        printf("- Is Gallant SC-6600:           %u\n",sb_card->is_gallant_sc6600);
        printf("- ESS chipset:                  %s\n",(sb_card->ess_extensions && ess_str != NULL) ? ess_str : "(none");

        printf("- ISA Plug & Play:              %s\n",sb_card->pnp_id != 0UL ? "Yes" : "No");
        if (sb_card->pnp_id != 0UL) {
            isa_pnp_product_id_to_str(ptmp,sb_card->pnp_id);
            printf("- ISA PnP ID:                   %s\n",ptmp);
        }
        if (sb_card->pnp_name != NULL)
            printf("- ISA PnP string:               %s\n",sb_card->pnp_name);
    }

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

