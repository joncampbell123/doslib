
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

unsigned char isa_pnp_read_config() {
	unsigned int patience = 20;
	unsigned char ret = 0xFF;

	do {
		isa_pnp_write_address(0x05);
		if (isa_pnp_read_data() & 1) {
			isa_pnp_write_address(0x04);
			ret = isa_pnp_read_data();
			break;
		}
		else {
			if (--patience == 0) break;
			t8254_wait(t8254_us2ticks(100));
		}
	} while (1);
	return ret;
}

#endif //!defined(TARGET_PC98)

