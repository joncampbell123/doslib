
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

const char *isa_pnp_device_category(uint32_t productid) {
	char tmp[8]; isa_pnp_product_id_to_str(tmp,productid);
	if (!memcmp(tmp,"PNP",3)) {
		switch (tmp[3]) {
			case '0': switch(tmp[4]) {
				case '0': return "System device, interrupt controller";
				case '1': return "System device, timer";
				case '2': return "System device, DMA controller";
				case '3': return "System device, keyboard";
				case '4': return "System device, parallel port";
				case '5': return "System device, serial port";
				case '6': return "System device, hard disk controller";
				case '7': return "System device, floppy disk controller";
				case '9': return "System device, display adapter";
				case 'A': return "System device, peripheral bus";
				case 'C': return "System device, motherboard device";
				case 'E': return "System device, PCMCIA controller";
				case 'F': return "System device, mouse";
				default:  return "System devices";
			}
			case '8': return "Network devices";
			case 'A': return "SCSI & non-standard CD-ROM devices";
			case 'B': return "Sound, video & multimedia";
			case 'C': return "Modem devices";
		};
	}

	return NULL;
}

