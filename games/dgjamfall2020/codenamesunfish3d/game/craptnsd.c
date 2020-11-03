
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
#include <malloc.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#include <hw/dos/himemsys.h>
#include <hw/vga/vga.h>
#include <hw/vga/vrl.h>
#include <hw/8237/8237.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/sndsb/sndsb.h>
#include <hw/adlib/adlib.h>
#include <fmt/minipng/minipng.h>

#include "timer.h"
#include "vmode.h"
#include "fonts.h"
#include "vrlimg.h"
#include "dbgheap.h"
#include "fontbmp.h"
#include "unicode.h"
#include "commtmp.h"
#include "sin2048.h"
#include "vrldraw.h"
#include "seqcomm.h"
#include "keyboard.h"
#include "dumbpack.h"
#include "fzlibdec.h"
#include "fataexit.h"
#include "sorcpack.h"
#include "rotozoom.h"
#include "seqcanvs.h"
#include "cutscene.h"
#include "craptn52.h"
#include "ldwavsn.h"

#include <hw/8042/8042.h>

/* Sound Blaster detection using hw/sndsb */
void detect_sound_blaster(void) {
    struct sndsb_ctx* ctx;

    /* First allow the user to control our detection with SET BLASTER=... in the environment block.
     * Since DOSBox/DOSBox-X usually sets BLASTER this means we'll use whatever I/O port, IRQ, and DMA
     * they assigned in dosbox.conf as well. */
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/ && sndsb_try_blaster_var() != NULL) {
        if (!sndsb_init_card(sndsb_card_blaster))
            sndsb_free_card(sndsb_card_blaster);
    }

    /* Otherwise, try the usual port 220h and port 240h most Sound Blaster cards are configured on,
     * but only if we didn't get anything with SET BLASTER=...  Port 220h is VERY COMMON, Port 240h
     * is much less common. */
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/)
        sndsb_try_base(0x220);
    if (sndsb_index_to_ctx(0)->baseio == 0/*NTS: currently does not return NULL*/)
        sndsb_try_base(0x240);

    /* Stop here if none detected */
    if ((ctx=sndsb_index_to_ctx(0))->baseio == 0/*NTS: currently does not return NULL*/)
        return;

    printf("Possible Sound Blaster detected at I/O port %xh\n",ctx->baseio);

    /* Autodetect the IRQ and DMA if not already obtained from SET BLASTER... */
    if (ctx->irq < 0)
        sndsb_probe_irq_F2(ctx);
    if (ctx->irq < 0)
        sndsb_probe_irq_80(ctx);
    if (ctx->dma8 < 0)
        sndsb_probe_dma8_E2(ctx);
    if (ctx->dma8 < 0)
        sndsb_probe_dma8_14(ctx);

    /* No IRQ/DMA, no sound. Not doing Goldplay or Direct DAC in *this* game, sorry */
    if (ctx->irq < 0 || ctx->dma8 < 0)
        return;

    /* Check card capabilities */
    sndsb_update_capabilities(ctx);
    sndsb_determine_ideal_dsp_play_method(ctx);

    /* Ok, accept */
    sound_blaster_ctx = ctx;

    printf("Found Sound Blaster at %xh IRQ %d DMA %d\n",sound_blaster_ctx->baseio,sound_blaster_ctx->irq,sound_blaster_ctx->dma8);

    sound_blaster_hook_irq();

    t8254_wait(t8254_us2ticks(1000000)); /* 1 second */
}

