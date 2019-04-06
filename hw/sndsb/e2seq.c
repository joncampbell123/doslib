
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
    unsigned char e2byte;

    if (common_sb_init() != 0)
        return 1;

    assert(sb_card != NULL);

    printf("What byte do I send with command 0xE2? "); fflush(stdout);
    {
        char line[64];
        if (fgets(line,sizeof(line),stdin) == NULL) return 1;
        e2byte = (unsigned char)strtoul(line,NULL,0);
    }
    printf("Sending 0x%02x\n",e2byte);

	sndsb_free_card(sb_card);
	free_sndsb(); /* will also de-ref/unhook the NMI reflection */
	return 0;
}

