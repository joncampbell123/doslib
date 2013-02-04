
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

uint16_t				isapnp_read_data = 0;
uint8_t					isapnp_probe_next_csn = 0;

const unsigned char isa_pnp_init_keystring[32] = {
	0x6A,0xB5,0xDA,0xED,0xF6,0xFB,0x7D,0xBE,
	0xDF,0x6F,0x37,0x1B,0x0D,0x86,0xC3,0x61,
	0xB0,0x58,0x2C,0x16,0x8B,0x45,0xA2,0xD1,
	0xE8,0x74,0x3A,0x9D,0xCE,0xE7,0x73,0x39
};

const char *isapnp_config_block_str[3] = {
	"Allocated",
	"Possible",
	"Compatible"
};

const char *isa_pnp_devd_pnp_0x00xx[] = {	/* PNP00xx */
	"AT interrupt controller",		/*      00 */
	"EISA interrupt controller",		/*      01 */
	"MCA interrupt controller",		/*      02 */
	"APIC",					/*      03 */
	"Cyrix SLiC MP interrupt controller"	/*      04 */
};

const char *isa_pnp_devd_pnp_0x01xx[] = {	/* PNP01xx */
	"AT Timer",				/*      00 */
	"EISA Timer",				/*      01 */
	"MCA Timer"				/*      02 */
};

const char *isa_pnp_devd_pnp_0x02xx[] = {	/* PNP02xx */
	"AT DMA Controller",			/*      00 */
	"EISA DMA Controller",			/*      01 */
	"MCA DMA Controller"			/*      02 */
};

const char *isa_pnp_devd_pnp_0x03xx[] = {				/* PNP03xx */
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

const char *isa_pnp_devd_pnp_0x04xx[] = {	/* PNP04xx */
	"Standard LPT printer port",		/*      00 */
	"ECP printer port"			/*      01 */
};

const char *isa_pnp_devd_pnp_0x05xx[] = {	/* PNP05xx */
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

const char *isa_pnp_devd_pnp_0x06xx[] = {	/* PNP06xx */
	"Generic ESDI/IDE/ATA controller",	/*      00 */
	"Plus Hardcard II",			/*      01 */
	"Plus Hardcard IIXL/EZ",		/*      02 */
	"Generic IDE, Microsoft Device Bay Specification"/* 03 */
};

const char *isa_pnp_devd_pnp_0x07xx[] = {			/* PNP07xx */
	"PC standard floppy disk controller",			/*      00 */
	"Standard floppy, Microsoft Device Bay Specification"	/*      01 */
};

const char *isa_pnp_devd_pnp_0x09xx[] = {			/* PNP09xx */
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

const char *isa_pnp_devd_pnp_0x0Cxx[] = {			/* PNP0Cxx */
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

const char *isa_pnp_devd_pnp_0x0Axx[] = {			/* PNP0Axx */
	"ISA bus",						/*      00 */
	"EISA bus",						/*      01 */
	"MCA bus",						/*      02 */
	"PCI bus",						/*      03 */
	"VESA bus",						/*      04 */
	"Generic ACPI bus",					/*      05 that's a bus? */
	"Generic ACPI extended IO bus"				/*      06 */
};

const char *isa_pnp_devd_pnp_0x0Exx[] = {			/* PNP0Exx */
	"Intel 82365-compatible PCMCIA controller",		/*      00 */
	"Cirrus Logic CL-PD6720 PCMCIA controller",		/*      01 */
	"VLSI VL82C146 PCMCIA controller",			/*      02 */
	"Intel 82365-compatible CardBus controller"		/*      03 */
};

const char *isa_pnp_devd_pnp_0x0Fxx[] = {			/* PNP0Fxx */
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

const char *isapnp_fml32_miosize_str[4] = {
	"8-bit",
	"16-bit",
	"8/16-bit",
	"32-bit"
};

const char *isapnp_sdf_priority_strs[3] = {
	"Good",
	"Acceptable",
	"Sub-optimal"
};

const char *isapnp_dma_speed_str[4] = {
	"Compatible",
	"Type A",
	"Type B",
	"Type F"
};

const char *isapnp_dma_xfer_preference_str[4] = {
	"8-bit",
	"8/16-bit",
	"16-bit",
	"??"
};

const char *isapnp_tag_strs[] = {
	/* small tags +0x00 */
	NULL,					/* 0x00 */
	"PnP version",
	"Logical device ID",			/* 0x02 */
	"Compatible device ID",
	"IRQ",					/* 0x04 */
	"DMA",
	"Dependent function start",		/* 0x06 */
	"Dependent function end",
	"I/O port",				/* 0x08 */
	"Fixed I/O port",
	NULL,					/* 0x0A */
	NULL,
	NULL,					/* 0x0C */
	NULL,
	"Vendor tag S-0xE",			/* 0x0E */
	"End",
	/* large tags +0x10 */
	NULL,					/* 0x10 */
	"Memory range",
	"Identifier string (ANSI)",		/* 0x12 */
	"Identifier string (Unicode)",
	"Vendor tag L-0x4",			/* 0x14 */
	"32-bit memory range",
	"32-bit fixed memory range"		/* 0x16 */
};

uint16_t			isa_pnp_bios_offset = 0;
struct isa_pnp_bios_struct	isa_pnp_info;
unsigned int			(__cdecl far *isa_pnp_rm_call)() = NULL;

#if TARGET_MSDOS == 32
uint16_t			isa_pnp_pm_code_seg = 0;	/* code segment to call protected mode BIOS if */
uint16_t			isa_pnp_pm_data_seg = 0;	/* data segment to call protected mode BIOS if */
uint8_t				isa_pnp_pm_use = 0;		/* 1=call protected mode interface  0=call real mode interface */
uint8_t				isa_pnp_pm_win95_mode = 0;	/* 1=call protected mode interface with 32-bit stack (Windows 95 style) */
uint16_t			isa_pnp_rm_call_area_code_sel = 0;
uint16_t			isa_pnp_rm_call_area_sel = 0;
void*				isa_pnp_rm_call_area = NULL;
#endif

int init_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	isa_pnp_rm_call_area_code_sel = 0;
	isa_pnp_rm_call_area_sel = 0;
	isa_pnp_pm_code_seg = 0;
	isa_pnp_pm_data_seg = 0;
	isa_pnp_pm_use = 0;
#endif
	return 1;
}

void free_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	if (isa_pnp_pm_code_seg != 0)
		dpmi_free_descriptor(isa_pnp_pm_code_seg);
	if (isa_pnp_pm_data_seg != 0)
		dpmi_free_descriptor(isa_pnp_pm_data_seg);
	if (isa_pnp_rm_call_area_code_sel != 0)
		dpmi_free_descriptor(isa_pnp_rm_call_area_code_sel);
	if (isa_pnp_rm_call_area_sel != 0)
		dpmi_free_dos(isa_pnp_rm_call_area_sel);
	isa_pnp_pm_code_seg = 0;
	isa_pnp_pm_data_seg = 0;
	isa_pnp_rm_call_area_sel = 0;
	isa_pnp_rm_call_area_code_sel = 0;
	isa_pnp_rm_call_area = NULL;
	isa_pnp_pm_use = 0;
#endif
}

int find_isa_pnp_bios() {
#if TARGET_MSDOS == 32
	uint8_t *scan = (uint8_t*)0xF0000;
#else
	uint8_t far *scan = (uint8_t far*)MK_FP(0xF000U,0x0000U);
#endif
	unsigned int offset,i;

	free_isa_pnp_bios();
	isa_pnp_bios_offset = (uint16_t)(~0U);
	memset(&isa_pnp_info,0,sizeof(isa_pnp_info));

	/* NTS: Stop at 0xFFE0 because 0xFFE0+0x21 >= end of 64K segment */
	for (offset=0U;offset != 0xFFE0U;offset += 0x10U,scan += 0x10) {
		if (scan[0] == '$' && scan[1] == 'P' && scan[2] == 'n' &&
			scan[3] == 'P' && scan[4] >= 0x10 && scan[5] >= 0x21) {
			uint8_t chk=0;
			for (i=0;i < scan[5];i++)
				chk += scan[i];

			if (chk == 0) {
				isa_pnp_bios_offset = (uint16_t)offset;
				_fmemcpy(&isa_pnp_info,scan,sizeof(isa_pnp_info));
				isa_pnp_rm_call = (void far*)MK_FP(isa_pnp_info.rm_ent_segment,
					isa_pnp_info.rm_ent_offset);

#if TARGET_MSDOS == 32
				isa_pnp_rm_call_area = dpmi_alloc_dos(ISA_PNP_RM_DOS_AREA_SIZE,&isa_pnp_rm_call_area_sel);
				if (isa_pnp_rm_call_area == NULL) {
					fprintf(stderr,"WARNING: Failed to allocate common area for DOS realmode calling\n");
					goto fail;
				}

				/* allocate descriptors to make calling the BIOS from pm mode */
				if ((isa_pnp_pm_code_seg = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_pm_code_seg,isa_pnp_info.pm_ent_segment_base)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_pm_code_seg,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_pm_code_seg,0x9A)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				if ((isa_pnp_pm_data_seg = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_pm_data_seg,isa_pnp_info.pm_ent_data_segment_base)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_pm_data_seg,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_pm_data_seg,0x92)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				/* allocate code selector for realmode area */
				if ((isa_pnp_rm_call_area_code_sel = dpmi_alloc_descriptor()) != 0) {
					if (!dpmi_set_segment_base(isa_pnp_rm_call_area_code_sel,(uint32_t)isa_pnp_rm_call_area)) {
						fprintf(stderr,"WARNING: Failed to set segment base\n"); goto fail; }
					if (!dpmi_set_segment_limit(isa_pnp_rm_call_area_code_sel,0xFFFFUL)) {
						fprintf(stderr,"WARNING: Failed to set segment limit\n"); goto fail; }
					if (!dpmi_set_segment_access(isa_pnp_rm_call_area_code_sel,0x9A)) {
						fprintf(stderr,"WARNING: Failed to set segment access rights\n"); goto fail; }
				}

				isa_pnp_pm_use = 1;
#endif

				return 1;
#if TARGET_MSDOS == 32
fail:				free_isa_pnp_bios();
				return 0;
#endif
			}
		}
	}

	return 0;
}

#if TARGET_MSDOS == 32

static unsigned int isa_pnp_thunk_and_call() { /* private, don't export */
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	if (isa_pnp_pm_use) {
		unsigned char *callfunc = (unsigned char*)isa_pnp_rm_call_area + 0x8;
		unsigned short ret_ax = ~0U,my_cs = 0;

		__asm {
			mov		ax,cs
			mov		my_cs,ax
		}

		/* 32-bit far pointer used in call below */
		*((uint32_t*)(callfunc+0+0)) = 0x08+0x8;
		*((uint32_t*)(callfunc+0+4)) = isa_pnp_rm_call_area_code_sel;
		/* 386 assembly language: CALL <proc> */
		*((uint8_t *)(callfunc+8+0)) = 0x9A;
		*((uint16_t*)(callfunc+8+1)) = (uint16_t)isa_pnp_info.pm_ent_offset;
		*((uint16_t*)(callfunc+8+3)) = (uint16_t)isa_pnp_pm_code_seg;
		/* 386 assembly language: from 16-bit segment: [32-bit] call far <32-bit code segment:offset of label "asshole"> */
		*((uint8_t *)(callfunc+8+5)) = 0x66;
		*((uint8_t *)(callfunc+8+6)) = 0x67;
		*((uint8_t *)(callfunc+8+7)) = 0xEA;
		*((uint32_t*)(callfunc+8+8)) = 0;
		*((uint32_t*)(callfunc+8+12)) = my_cs;

		/* protected mode call */
		if (isa_pnp_pm_win95_mode) {
			/* Windows 95 mode: call just like below, but DON'T switch stacks (leave SS:ESP => 32-bit stack).
			 * This is what Windows 95 does and it's probably why the Bochs implementation of Plug & Play BIOS
			 * doesn't work, since everything in the PnP spec implies 16-bit FAR pointers */
			__asm {
				pushad
				push		fs
				cli

				; These stupid acrobatics are necessary because Watcom has a hang-up
				; about inline assembly referring to stack variables and doesn't like
				; inline assembly referring to label addresses
				call		acrobat1			; near call to put label address on stack
				jmp		asshole				; relative JMP address to the 'asshole' label below
acrobat1:			pop		eax				; retrieve the address of the 'acrobat1' label
				add		eax,dword ptr [eax+1]		; sum it against the relative 32-bit address portion of the JMP instruction (NTS: JMP = 0xEA <32-bit rel addr>)
				mov		esi,callfunc
				mov		dword ptr [esi+8+8],eax

;				mov		cx,ss
				mov		dx,ds
				mov		ebx,esp
;				mov		esi,ebp
;				xor		ebp,ebp
				mov		esp,isa_pnp_rm_call_area
				add		esp,0x1F0
				mov		ax,isa_pnp_rm_call_area_sel
				mov		fs,ax
				mov		ax,isa_pnp_pm_data_seg
				mov		ds,ax
				mov		es,ax

				; call
				jmpf		fword ptr fs:0x8

				; WARNING: do NOT remove these NOPs
				nop
				nop
				nop
				nop
				nop
				nop
asshole:
				nop
				nop
				nop
				nop
				nop
				nop
				nop

				; restore stack (NTS: PnP BIOSes are supposed to restore ALL regs except AX)
				cli
;				mov		ebp,esi
				mov		esp,ebx
;				mov		ss,cx
				mov		ds,dx
				mov		es,dx

				mov		ret_ax,ax

				sti
				pop		fs
				popad
			}
		}
		else {
			__asm {
				pushad
				cli

				; These stupid acrobatics are necessary because Watcom has a hang-up
				; about inline assembly referring to stack variables and doesn't like
				; inline assembly referring to label addresses
				call		acrobat1			; near call to put label address on stack
				jmp		asshole				; relative JMP address to the 'asshole' label below
acrobat1:			pop		eax				; retrieve the address of the 'acrobat1' label
				add		eax,dword ptr [eax+1]		; sum it against the relative 32-bit address portion of the JMP instruction (NTS: JMP = 0xEA <32-bit rel addr>)
				mov		esi,callfunc
				mov		dword ptr [esi+8+8],eax

				mov		cx,ss
				mov		dx,ds
				mov		ebx,esp
				mov		esi,ebp
				xor		ebp,ebp
				mov		esp,0x1F0
				mov		ax,isa_pnp_rm_call_area_sel
				mov		ss,ax
				mov		ax,isa_pnp_pm_data_seg
				mov		ds,ax
				mov		es,ax

				; call
				jmpf		fword ptr ss:0x8

				; WARNING: do NOT remove these NOPs
				nop
				nop
				nop
				nop
				nop
				nop
asshole:
				nop
				nop
				nop
				nop
				nop
				nop
				nop

				; restore stack (NTS: PnP BIOSes are supposed to restore ALL regs except AX)
				cli
				mov		ebp,esi
				mov		esp,ebx
				mov		ss,cx
				mov		ds,dx
				mov		es,dx

				mov		ret_ax,ax

				sti
				popad
			}
		}

		return ret_ax;
	}
	else {
		/* real-mode call via DPMI */
		/* ..but wait---we might be running under DOS4/G which doesn't
		   provide the "real mode call" function! */
		struct dpmi_realmode_call d={0};
		unsigned int x = (unsigned int)(&d); /* NTS: Work around Watcom's "cannot take address of stack symbol" error */
		d.cs = isa_pnp_info.rm_ent_segment;
		d.ip = isa_pnp_info.rm_ent_offset;
		d.ss = sgm;				/* our real-mode segment is also the stack during the call */
		d.sp = 0x1F0;

		if (dpmi_no_0301h < 0) probe_dpmi();

		if (dpmi_no_0301h > 0) {
			/* Fuck you DOS4/GW! */
			dpmi_alternate_rm_call_stacko(&d);
		}
		else {
			__asm {
				pushad

				push		ds
				pop		es

				mov		eax,0x0301		; DPMI call real-mode far routine
				xor		ebx,ebx
				xor		ecx,ecx
				mov		edi,x
				int		0x31

				popad
			}
		}

		if (!(d.flags & 1))
			return d.eax&0xFF;
	}

	return ~0U;
}

unsigned int isa_pnp_bios_number_of_sysdev_nodes(unsigned char far *a,unsigned int far *b) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a
	 * SEG:0x0004 = b
	 * SEG:0x00F0 = stack for passing params */
	unsigned int *rb_a = (unsigned int*)((unsigned char*)isa_pnp_rm_call_area);
	unsigned int *rb_b = (unsigned int*)((unsigned char*)isa_pnp_rm_call_area + 4);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = ~0UL; *rb_b = ~0UL;

	/* build the stack */
	stk[5] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[4] = sgm;
	stk[3] = 4;	/* offset of "b" (segment will be filled in separately) */
	stk[2] = sgm;
	stk[1] = 0;	/* offset of "a" (ditto) */
	stk[0] = 0;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		*a = (unsigned char)(*rb_a);
		*b = *rb_b & 0xFFFF;
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_get_sysdev_node(unsigned char far *a,unsigned char far *b,unsigned int c) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned char *rb_a = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area);
	unsigned char *rb_b = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = *a;
	rb_b[0] = rb_b[1] = 0;

	stk[6] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[5] = c;
	stk[4] = sgm;	/* offset of "b" */
	stk[3] = 0x200;
	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0;
	stk[0] = 1;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		unsigned int len = *((uint16_t*)rb_b);	/* read back the size of the device node */
		*a = (unsigned char)(*rb_a);
		if ((0x100+len) > ISA_PNP_RM_DOS_AREA_SIZE) {
			fprintf(stderr,"Whoahhhh..... the returned device struct is too large! len=%u\n",len);
			return 0xFF;
		}
		else {
			if ((ret_ax&0xFF) == 0) _fmemcpy(b,rb_b,len);
		}
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_get_static_alloc_resinfo(unsigned char far *a) {
	return ~0;
}

unsigned int isa_pnp_bios_get_pnp_isa_cfg(unsigned char far *a) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned char *rb_a = (unsigned char*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	stk[3] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0x200;
	stk[0] = 0x40;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		if ((ret_ax&0xFF) == 0) _fmemcpy(a,rb_a,sizeof(struct isapnp_pnp_isa_cfg));
		return ret_ax&0xFF;
	}

	return ~0;
}

/* UPDATE 2011/05/27: Holy shit, found an ancient Pentium Pro system who's BIOS implements the ESCD functions! */
unsigned int isa_pnp_bios_get_escd_info(unsigned int far *min_escd_write_size,unsigned int far *escd_size,unsigned long far *nv_storage_base) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned short *rb_a = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x200);
	unsigned short *rb_b = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x204);
	unsigned long  *rb_c = (unsigned long*)((unsigned char*)isa_pnp_rm_call_area + 0x208);
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned short sgm = isa_pnp_pm_use ? (unsigned short)isa_pnp_rm_call_area_sel : (unsigned short)((size_t)isa_pnp_rm_call_area >> 4);
	unsigned int ret_ax = ~0;

	*rb_a = ~0UL;
	*rb_b = ~0UL;
	*rb_c = ~0UL;

	stk[7] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */

	stk[6] = sgm;	/* offset of "c" */
	stk[5] = 0x208;

	stk[4] = sgm;	/* offset of "b" */
	stk[3] = 0x204;

	stk[2] = sgm;	/* offset of "a" */
	stk[1] = 0x200;

	stk[0] = 0x41;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		if ((ret_ax&0xFF) == 0) {
			*min_escd_write_size = *rb_a;
			*escd_size = *rb_b;
			*nv_storage_base = *rb_c;
		}
		return ret_ax&0xFF;
	}

	return ~0;
}

unsigned int isa_pnp_bios_send_message(unsigned int msg) {
	/* use the DOS segment we allocated as a "trampoline" for the 16-bit code to write the return values to */
	/* SEG:0x0000 = a (node)
	 * SEG:0x00F0 = stack for passing params
	 * SEG:0x0100 = b (buffer for xfer)
	 * SEF:0x0FFF = end */
	unsigned short *stk = (unsigned short*)((unsigned char*)isa_pnp_rm_call_area + 0x1F0);
	unsigned int ret_ax = ~0;

	stk[2] = isa_pnp_pm_use ? (unsigned short)isa_pnp_pm_data_seg : (unsigned short)isa_pnp_info.rm_ent_data_segment; /* BIOS data segment */
	stk[1] = msg;

	stk[0] = 0x04;	/* function */

	ret_ax = isa_pnp_thunk_and_call();
	if (ret_ax == ~0U) {
		fprintf(stderr,"WARNING: Thunk and call failed\n");
	}
	else {
		return ret_ax&0xFF;
	}

	return ~0U;
}

#endif

void isa_pnp_product_id_to_str(char *str,unsigned long id) {
	sprintf(str,"%c%c%c%X%X%X%X",
		(unsigned char)(0x40+((id>>2)&0x1F)),
		(unsigned char)(0x40+((id&3)<<3)+((id>>13)&7)),
		(unsigned char)(0x40+((id>>8)&0x1F)),
		(unsigned char)((id>>20)&0xF),
		(unsigned char)((id>>16)&0xF),
		(unsigned char)((id>>28)&0xF),
		(unsigned char)((id>>24)&0xF));
}

const char *isapnp_tag_str(uint8_t tag) { /* tag from struct isapnp_tag NOT the raw byte */
	if (tag >= (sizeof(isapnp_tag_strs)/sizeof(isapnp_tag_strs[0])))
		return NULL;

	return isapnp_tag_strs[tag];
}

const char *isapnp_sdf_priority_str(uint8_t x) {
	if (x >= 3) return NULL;
	return isapnp_sdf_priority_strs[x];
}

int isapnp_read_tag(uint8_t far **pptr,uint8_t far *fence,struct isapnp_tag *tag) {
	uint8_t far *ptr = *pptr;
	unsigned char c;

	if (ptr >= fence)
		return 0;

	c = *ptr++;
	if (c & 0x80) { /* large tag */
		tag->tag = 0x10 + (c & 0x7F);
		tag->len = *((uint16_t far*)(ptr));
		ptr += 2;
		tag->data = ptr;
		ptr += tag->len;
		if (ptr > fence)
			return 0;
	}
	else { /* small tag */
		tag->tag = (c >> 3) & 0xF;
		tag->len = c & 7;
		tag->data = ptr;
		ptr += tag->len;
		if (ptr > fence)
			return 0;
	}

	*pptr = ptr;
	return 1;
}

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

void isa_pnp_wake_csn(unsigned char id) {
	isa_pnp_write_address(3); /* Wake[CSN] */
	isa_pnp_write_data(id); /* isolation state */
}

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

void isa_pnp_set_read_data_port(uint16_t port) {
	isa_pnp_write_address(0x00);	/* RD_DATA port */
	isa_pnp_write_data(port >> 2);
}

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

uint8_t isa_pnp_read_data_register(uint8_t x) {
	isa_pnp_write_address(x);
	return isa_pnp_read_data();
}

void isa_pnp_write_data_register(uint8_t x,uint8_t data) {
	isa_pnp_write_address(0x00);
	isa_pnp_write_address(x);
	isa_pnp_write_data(data);
}

int isa_pnp_read_io_resource(unsigned int i) {
	uint16_t ret;

	if (i >= 8) return -1;
	ret  = (uint16_t)isa_pnp_read_data_register(0x60 + (i*2)) << 8;
	ret |= (uint16_t)isa_pnp_read_data_register(0x60 + (i*2) + 1);
	return ret;
}

void isa_pnp_write_io_resource(unsigned int i,uint16_t port) {
	if (i >= 8) return;
	isa_pnp_write_data_register(0x60 + (i*2),port >> 8);
	isa_pnp_write_data_register(0x60 + (i*2) + 1,port);
	t8254_wait(t8254_us2ticks(250));
}

int isa_pnp_read_irq(unsigned int i) {
	uint8_t c;

	if (i >= 2) return -1;
	c = isa_pnp_read_data_register(0x70 + (i*2));
	if (c == 0xFF) return -1;
	if ((c & 15) == 0) return -1;	/* not assigned */
	return (c & 15);
}

void isa_pnp_write_irq(unsigned int i,int irq) {
	if (i >= 2) return;
	if (irq < 0) irq = 0;
	isa_pnp_write_data_register(0x70 + (i*2),irq);
	t8254_wait(t8254_us2ticks(250));
}

void isa_pnp_write_irq_mode(unsigned int i,unsigned int im) {
	if (i >= 2) return;
	isa_pnp_write_data_register(0x70 + (i*2) + 1,im);
	t8254_wait(t8254_us2ticks(250));
}

int isa_pnp_read_dma(unsigned int i) {
	uint8_t c;

	if (i >= 2) return -1;
	c = isa_pnp_read_data_register(0x74 + i);
	if (c == 0xFF) return -1;
	if ((c & 7) == 4) return -1;	/* not assigned */
	return (c & 7);
}

void isa_pnp_write_dma(unsigned int i,int dma) {
	if (i >= 2) return;
	if (dma < 0) dma = 4;
	isa_pnp_write_data_register(0x74 + i,dma);
	t8254_wait(t8254_us2ticks(250));
}
