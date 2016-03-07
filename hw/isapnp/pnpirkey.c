
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

/* the caller is expected to specify which card by calling isa_pnp_wake_csn() */
int isa_pnp_init_key_readback(unsigned char *data/*9 bytes*/) {
	unsigned char checksum = 0x6a;
	unsigned char b,c1,c2,bit;
	unsigned int i,j;

	isa_pnp_write_address(1); /* serial isolation reg */
	for (i=0;i < 9;i++) {
		b = 0;
		for (j=0;j < 8;j++) {
			c1 = isa_pnp_read_data();
			c2 = isa_pnp_read_data();
			if (c1 == 0x55 && c2 == 0xAA) {
				b |= 1 << j;
				bit = 1;
			}
			else {
				bit = 0;
			}

			if (i < 8)
				checksum = ((((checksum ^ (checksum >> 1)) & 1) ^ bit) << 7) | (checksum >> 1);

			t8254_wait(t8254_us2ticks(275));
		}
		data[i] = b;
	}

	return (checksum == data[8]);
}

