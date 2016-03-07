
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

const char *isa_pnp_device_type(const uint8_t far *b,const char **subtype,const char **inttype) {
	const char *st = NULL;
	const char *it = NULL;
	const char *t = NULL;

	switch (b[0]) {
		case 1:	t = "Mass Storage Device";
			switch (b[1]) {
				case 0: st = "SCSI controller"; break;
				case 1: st = "IDE controller (Standard ATA compatible)";
					switch (b[2]) {
						case 0: it = "Generic IDE"; break;
					};
					break;
				case 2: st = "Floppy controller (Standard 765 compatible)";
					switch (b[2]) {
						case 0: it = "Generic floppy"; break;
					};
					break;
				case 3: st = "IPI controller";
					switch (b[2]) {
						case 0: it = "Generic"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 2:	t = "Network Interface Controller";
			switch (b[1]) {
				case 0: st = "Ethernet controller";
					switch (b[2]) {
						case 0: it = "Generic"; break;
					};
					break;
				case 1: st = "Token ring controller";
					switch (b[2]) {
						case 0: it = "Generic"; break;
					};
					break;
				case 2: st = "FDDI controller";
					switch (b[2]) {
						case 0: it = "Generic"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 3:	t = "Display Controller";
			switch (b[1]) {
				case 0: st = "VGA controller (Standard VGA compatible)";
					switch (b[2]) {
						case 0: it = "Generic VGA compatible"; break;
						case 1: it = "VESA SVGA compatible"; break;
					};
					break;
				case 1: st = "XGA compatible controller";
					switch (b[2]) {
						case 0: it = "General XGA compatible"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 4:	t = "Multimedia Controller";
			switch (b[1]) {
				case 0: st = "Video controller";
					switch (b[2]) {
						case 0: it = "General video"; break;
					};
					break;
				case 1: st = "Audio controller";
					switch (b[2]) {
						case 0: it = "General audio"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 5:	t = "Memory";
			switch (b[1]) {
				case 0: st = "RAM";
					switch (b[2]) {
						case 0: it = "General RAM"; break;
					};
					break;
				case 1: st = "Flash memory";
					switch (b[2]) {
						case 0: it = "General flash memory"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 6:	t = "Bridge controller";
			switch (b[1]) {
				case 0: st = "Host processor bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 1: st = "ISA bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 2: st = "EISA bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 3: st = "MCA bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 4: st = "PCI bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 5: st = "PCMCIA bridge";
					switch (b[2]) {
						case 0: it = "General"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 7:	t = "Communications device";
			switch (b[1]) {
				case 0: st = "RS-232";
					switch (b[2]) {
						case 0: it = "Generic"; break;
						case 1: it = "16450-compatible"; break;
						case 2: it = "16550-compatible"; break;
					};
					break;
				case 1: st = "Parallel port (AT compatible)";
					switch (b[2]) {
						case 0: it = "General"; break;
						case 1: it = "Model 30 bidirectional port"; break;
						case 2: it = "ECP 1.x port"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 8:	t = "System peripheral";
			switch (b[1]) {
				case 0: st = "Programmable Interrupt Controller (8259 compatible)";
					switch (b[2]) {
						case 0: it = "Generic"; break;
						case 1: it = "ISA"; break;
						case 2: it = "EISA"; break;
					};
					break;
				case 1: st = "DMA controller (8237 compatible)";
					switch (b[2]) {
						case 0: it = "General"; break;
						case 1: it = "ISA"; break;
						case 2: it = "EISA"; break;
					};
					break;
				case 2: st = "System Timer (8254 compatible)";
					switch (b[2]) {
						case 0: it = "General"; break;
						case 1: it = "ISA"; break;
						case 2: it = "EISA"; break;
					};
					break;
				case 3: st = "Real-time clock";
					switch (b[2]) {
						case 0: it = "Generic"; break;
						case 1: it = "ISA"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 9:	t = "Input device";
			switch (b[1]) {
				case 0: st = "Keyboard controller";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 1: st = "Digitizer (pen)";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 2: st = "Mouse controller";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 10: t = "Docking station";
			switch (b[1]) {
				case 0: st = "Generic";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;
		case 11: t = "CPU type";
			switch (b[1]) {
				case 0: st = "386";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 1: st = "486";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 2: st = "586";
					switch (b[2]) {
						case 0: it = "N/A"; break;
					};
					break;
				case 0x80:
					st = "Other";
					break;
			};
			break;

	};

	if (subtype != NULL)
		*subtype = st;
	if (inttype != NULL)
		*inttype = it;

	return t;
}

