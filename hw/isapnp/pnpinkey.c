
#if !defined(TARGET_PC98)

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <dos/dos.h>
#include <isapnp/isapnp.h>

#include <hw/8254/8254.h>		/* 8254 timer */

const unsigned char isa_pnp_init_keystring[32] = {
	0x6A,0xB5,0xDA,0xED,0xF6,0xFB,0x7D,0xBE,
	0xDF,0x6F,0x37,0x1B,0x0D,0x86,0xC3,0x61,
	0xB0,0x58,0x2C,0x16,0x8B,0x45,0xA2,0xD1,
	0xE8,0x74,0x3A,0x9D,0xCE,0xE7,0x73,0x39
};

void isa_pnp_init_key() {
	unsigned int i;

	for (i=0;i < 4;i++)
		isa_pnp_write_address(0);
	for (i=0;i < 32;i++)
		isa_pnp_write_address(isa_pnp_init_keystring[i]);

	/* software must delay 1ms prior to starting the first pair of isolation reads and must wait
	 * 250us between each subsequent pair of isolation reads. this delay gives the ISA card time
	 * to access information for possibly very slow storage devices */
	t8254_wait(t8254_us2ticks(1200));
}

#endif //!defined(TARGET_PC98)

