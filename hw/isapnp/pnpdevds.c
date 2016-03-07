
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

static const char *isa_pnp_devd_pnp_0x00xx[] = {/* PNP00xx */
	"AT interrupt controller",		/*      00 */
	"EISA interrupt controller",		/*      01 */
	"MCA interrupt controller",		/*      02 */
	"APIC",					/*      03 */
	"Cyrix SLiC MP interrupt controller"	/*      04 */
};

static const char *isa_pnp_devd_pnp_0x01xx[] = {/* PNP01xx */
	"AT Timer",				/*      00 */
	"EISA Timer",				/*      01 */
	"MCA Timer"				/*      02 */
};

static const char *isa_pnp_devd_pnp_0x02xx[] = {/* PNP02xx */
	"AT DMA Controller",			/*      00 */
	"EISA DMA Controller",			/*      01 */
	"MCA DMA Controller"			/*      02 */
};

static const char *isa_pnp_devd_pnp_0x03xx[] = {			/* PNP03xx */
	"IBM PC/XT keyboard controller (83-key)",			/*      00 */
	"IBM PC/AT keyboard controller (86-key)",			/*      01 */
	"IBM PC/XT keyboard controller (84-key)",			/*      02 */
	"IBM Enhanced (101/102-key, PS/2 mouse support)",		/*      03 */
	"Olivetti Keyboard (83-key)",					/*      04 */
	"Olivetti Keyboard (102-key)",					/*      05 */
	"Olivetti Keyboard (86-key)",					/*      06 */
	"Microsoft Windows(R) Keyboard",				/*      07 */
	"General Input Device Emulation Interface (GIDEI) legacy",	/*      08 */
	"Olivetti Keyboard (A101/102 key)",				/*      09 */
	"AT&T 302 keyboard",						/*      0A */
	"Reserved by Microsoft",					/*      0B */
	NULL,								/*      0C */
	NULL,								/*      0D */
	NULL,								/*      0E */
	NULL,								/*      0F */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,			/*      10-17 */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,			/*      18-1F */
	"Japanese 106-key keyboard A01",				/*      20 */
	"Japanese 101-key keyboard",					/*      21 */
	"Japanese AX keyboard",						/*      22 */
	"Japanese 106-key keyboard 002/003",				/*      23 */
	"Japanese 106-key keyboard 001",				/*      24 */
	"Japanese Toshiba Desktop keyboard",				/*      25 */
	"Japanese Toshiba Laptop keyboard",				/*      26 */
	"Japanese Toshiba Notebook keyboard",				/*      27 */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,			/*      28-2F */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,			/*      30-37 */
	NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,			/*      38-3F */
	"Korean 84-key keyboard",					/*      40 */
	"Korean 86-key keyboard",					/*      41 */
	"Korean Enhanced keyboard",					/*      42 */
	"Korean Enhanced keyboard 101c",				/*      43 */
	"Korean Enhanced keyboard 103"					/*      44 */
};

static const char *isa_pnp_devd_pnp_0x04xx[] = {/* PNP04xx */
	"Standard LPT printer port",		/*      00 */
	"ECP printer port"			/*      01 */
};

static const char *isa_pnp_devd_pnp_0x05xx[] = {/* PNP05xx */
	"Standard PC COM port",			/*      00 */
	"16550A-compatible COM port",		/*      01 */
	"Multiport serial device",		/*      02 */
	NULL,					/*      03 */
	NULL,					/*      04 */
	NULL,					/*      05 */
	NULL,					/*      06 */
	NULL,					/*      07 */
	NULL,					/*      08 */
	NULL,					/*      09 */
	NULL,					/*      0A */
	NULL,					/*      0B */
	NULL,					/*      0C */
	NULL,					/*      0D */
	NULL,					/*      0E */
	NULL,					/*      0F */
	"Generic IRDA-compatible",		/*      10 */
	"Generic IRDA-compatible"		/*      11 */
};

static const char *isa_pnp_devd_pnp_0x06xx[] = {/* PNP06xx */
	"Generic ESDI/IDE/ATA controller",	/*      00 */
	"Plus Hardcard II",			/*      01 */
	"Plus Hardcard IIXL/EZ",		/*      02 */
	"Generic IDE, Microsoft Device Bay Specification"/* 03 */
};

static const char *isa_pnp_devd_pnp_0x07xx[] = {		/* PNP07xx */
	"PC standard floppy disk controller",			/*      00 */
	"Standard floppy, Microsoft Device Bay Specification"	/*      01 */
};

static const char *isa_pnp_devd_pnp_0x09xx[] = {		/* PNP09xx */
	"VGA compatible",					/*      00 */
	"Video Seven VRAM/VRAM II/1024i",			/*      01 */
	"8514/A compatible",					/*      02 */
	"Trident VGA",						/*      03 */
	"Cirrus Logic Laptop VGA",				/*      04 */
	"Cirrus Logic VGA",					/*      05 */
	"Tseng ET4000",						/*      06 */
	"Western Digital VGA",					/*      07 */
	"Western Digital Laptop VGA",				/*      08 */
	"S3 911/924",						/*      09 */
	"ATI Ultra Pro/Plus Mach32",				/*      0A */
	"ATI Ultra Mach8",					/*      0B */
	"XGA compatible",					/*      0C */
	"ATI VGA wonder",					/*      0D */
	"Weitek P9000 graphics",				/*      0E */
	"Oak VGA",						/*      0F */

	"Compaq QVision",					/*      10 */
	"XGA/2",						/*      11 */
	"Tseng W32/W32i/W32p",					/*      12 */
	"S3 801/928/964",					/*      13 */
	"Cirrus 5429/5434",					/*      14 */
	"Compaq advanced VGA",					/*      15 */
	"ATI Ultra Pro Turbo Mach64",				/*      16 */
	"Microsoft Reserved",					/*      17 */
	"Matrox",						/*      18 */
	"Compaq QVision 2000",					/*      19 */
	"Tseng W128",						/*      1A */
	NULL,NULL,NULL,NULL,NULL,				/*      1B-1F */

	NULL,NULL,NULL,NULL, NULL,NULL,NULL,NULL,		/*      20-27 */
	NULL,NULL,NULL,NULL, NULL,NULL,NULL,NULL,		/*      28-2F */

	"Chips & Technologies SVGA",				/*      30 */
	"Chips & Technologies accelerator",			/*      31 */
	NULL,NULL, NULL,NULL,NULL,NULL,				/*      32-37 */
	NULL,NULL,NULL,NULL, NULL,NULL,NULL,NULL,		/*      38-3F */

	"NCR 77c22e SVGA",					/*      40 */
	"NCR 77c32blt"						/*      41 */
};

static const char *isa_pnp_devd_pnp_0x0Cxx[] = {		/* PNP0Cxx */
	"Plug & Play BIOS",					/*      00 */
	"System board",						/*      01 */
	"Motherboard resources",				/*      02 */
	"Plug & Play BIOS event interrupt",			/*      03 */
	"Math coprocessor",					/*      04 */
	"APM BIOS",						/*      05 */
	"(early PnP #06)",					/*      06 */
	"(early PnP #07)",					/*      07 */
	"ACPI system board",					/*      08 */
	"ACPI embedded controller",				/*      09 */
	"ACPI control method battery",				/*      0A */
	"ACPI fan",						/*      0B */
	"ACPI power button",					/*      0C */
	"ACPI lid",						/*      0D */
	"ACPI sleep button",					/*      0E */
	"PCI interrupt link",					/*      0F */
	"ACPI system indicator",				/*      10 */
	"ACPI thermal zone",					/*      11 */
	"Device bay controller",				/*      12 */
	"Plug & Play BIOS (non-ACPI?)"				/*      13 */
};

static const char *isa_pnp_devd_pnp_0x0Axx[] = {		/* PNP0Axx */
	"ISA bus",						/*      00 */
	"EISA bus",						/*      01 */
	"MCA bus",						/*      02 */
	"PCI bus",						/*      03 */
	"VESA bus",						/*      04 */
	"Generic ACPI bus",					/*      05 that's a bus? */
	"Generic ACPI extended IO bus"				/*      06 */
};

static const char *isa_pnp_devd_pnp_0x0Exx[] = {		/* PNP0Exx */
	"Intel 82365-compatible PCMCIA controller",		/*      00 */
	"Cirrus Logic CL-PD6720 PCMCIA controller",		/*      01 */
	"VLSI VL82C146 PCMCIA controller",			/*      02 */
	"Intel 82365-compatible CardBus controller"		/*      03 */
};

static const char *isa_pnp_devd_pnp_0x0Fxx[] = {		/* PNP0Fxx */
	"Microsoft bus mouse",					/*      00 */
	"Microsoft serial",					/*      01 */
	"Microsoft InPort",					/*      02 */
	"Microsoft PS/2",					/*      03 */
	"Mouse systems",					/*      04 */
	"Mouse systems 3-button (COM2)",			/*      05 */
	"Genius mouse (COM1)",					/*      06 */
	"Genius mouse (COM2)",					/*      07 */
	"Logitech serial",					/*      08 */
	"Microsoft BallPoint serial",				/*      09 */
	"Microsoft plug & play",				/*      0A */
	"Microsoft plug & play BallPoint",			/*      0B */
	"Microsoft compatible serial",				/*      0C */
	"Microsoft compatible InPort compatible",		/*      0D */
	"Microsoft compatible PS/2",				/*      0E */
	"Microsoft compatible serial BallPoint compatible",	/*      0F */
	"Texas Instruments QuickPort",				/*      10 */
	"Microsoft compatible bus mouse",			/*      11 */
	"Logitech PS/2",					/*      12 */
	"PS/2 port for PS/2 mice",				/*      13 */
	"Microsoft kids mouse",					/*      14 */
	"Logitech bus mouse",					/*      15 */
	"Logitech swift device",				/*      16 */
	"Logitech compatible serial",				/*      17 */
	"Logitech compatible bus mouse",			/*      18 */
	"Logitech compatible PS/2",				/*      19 */
	"Logitech compatible swift",				/*      1A */
	"HP Omnibook mouse",					/*      1B */
	"Compaq LTE trackball PS/2",				/*      1C */
	"Compaq LTE trackball serial",				/*      1D */
	"Microsoft kids trackball",				/*      1E */
	NULL							/*      1F */
};

void isa_pnp_device_description(char desc[255],uint32_t productid) {
	char tmp[8]; isa_pnp_product_id_to_str(tmp,productid);
	desc[0] = 0;

	if (!memcmp(tmp,"PNP",3)) {
		unsigned int hex = (unsigned int)strtol(tmp+3,0,16),hexm = 0;
		const char *rets = NULL,**arr = NULL;
		unsigned int ars = 0;
		if (hex == 0x0800) {
			rets = "PC speaker";
		}
		else if (hex == 0x0B00) {
			rets = "Realtime clock";
		}
		else if (hex == 0x09FF) {
			rets = "Plug & Play DDC monitor";
		}
		else {
			switch (hex>>8) {
#define PNP(a) \
	case a: ars = sizeof(isa_pnp_devd_pnp_##a##xx)/sizeof(isa_pnp_devd_pnp_##a##xx[0]); arr = isa_pnp_devd_pnp_##a##xx; hexm = 0xFF; break
				PNP(0x00); /* PNP00xx */	PNP(0x01); /* PNP01xx */
				PNP(0x02); /* PNP02xx */	PNP(0x03); /* PNP03xx */
				PNP(0x04); /* PNP04xx */	PNP(0x05); /* PNP05xx */
				PNP(0x06); /* PNP06xx */	PNP(0x07); /* PNP07xx */
				PNP(0x09); /* PNP09xx */	PNP(0x0A); /* PNP0Axx */
				PNP(0x0C); /* PNP0Cxx */	PNP(0x0E); /* PNP0Exx */
				PNP(0x0F); /* PNP0Fxx */
#undef PNP
			}
		};

		if (rets == NULL && ars != (size_t)0 && arr != NULL && (hex&hexm) < ars)
			rets = arr[hex&hexm];

		if (rets != NULL)
			strcpy(desc,rets);
	}
}

