
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/pci/pci.h>
#include <hw/8254/8254.h>		/* 8254 timer */

struct pci_ref {
	uint8_t bus,dev,func;
	uint16_t vendor,device;
};

struct pci_ref pci_isa_bridge={0xFF,0xFF,0xFF};

enum {
	CMD_DUMPRAW=0,
	CMD_DUMP
};

static void help() {
	fprintf(stderr,"test [options]\n");
	fprintf(stderr,"  Motherboard toy for Intel PIIX3 based motherboards\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"WARNING: This program allows you to play with the PIIX3 chipset\n");
	fprintf(stderr,"         on your motherboard, even in ways that can crash or hang\n");
	fprintf(stderr,"         your system and necessitate pushing the reset button.\n");
	fprintf(stderr,"         The responsibility is yours to use this wisely!\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"  /d           Dump PCI configuration space\n");
	fprintf(stderr,"  /dr          Dump PCI configuration space, raw\n");
	fprintf(stderr,"  /ch          List switches to change parameters\n");
}

void pause() {
	while (getch() != 13);
}

#define PAGEME {\
	if (++lines >= 23) {\
		lines = 0;\
		pause();\
	}\
}

static void ch_help() {
	int lines=0;

	fprintf(stderr,"Changeable params, PCI-ISA bridge\n"); PAGEME;
	fprintf(stderr," [!] = The command may disrupt your ability to use the normal\n");
	fprintf(stderr,"       DOS environment and/or cause a crash.\n");
	fprintf(stderr,"  /pi:dma-alias=<1|0>    Enable/disable DMA Reserved Page Reg Alias\n"); PAGEME;
	fprintf(stderr,"  /pi:io8r=<N>           8-bit I/O recovery time (0 to disable)\n"); PAGEME;
	fprintf(stderr,"  /pi:io16r=<N>          16-bit I/O recovery time (0 to disable)\n"); PAGEME;
	fprintf(stderr,"  /pi:apic-cs=<1|0>      Enable/disable APIC Chip Select\n");
	fprintf(stderr,"  /pi:extbios=<1|0>      Enable/disable Extended BIOS\n");
	fprintf(stderr,"  /pi:lowbios=<1|0>      Enable/disable lower BIOS [!]\n");
	fprintf(stderr,"  /pi:copren=<1|0>       Enable/disable coprocessor error on IRQ 13\n");
	fprintf(stderr,"  /pi:ps2irq=<1|0>       Enable/disable PS/2 mouse on IRQ 12\n");
	fprintf(stderr,"  /pi:bioswrite=<1|0>    Enable/disable asserting BIOSCS# on write\n");
	fprintf(stderr,"  /pi:keyio=<1|0>        Enable/disable chipset I/O of Keyboard [!]\n");
	fprintf(stderr,"  /pi:rtio=<1|0>         Enable/disable chipset I/O of RTC\n");
	fprintf(stderr,"  /pi:pci-irq-<x>=<N>    Route PCI IRQ x to IRQ N where 'x' is A,B,C or D\n");
	fprintf(stderr,"                         and N is 1...15, or 0 to disable/assign to ISA [!]\n");
}

static const char *s_yesno(int b) {
	return b ? "Yes" : "No";
}

#define PI_MAX 8
static int pi_args=0;
static const char *pi_arg[PI_MAX];

int main(int argc,char **argv) {
	int cmd = -1,i,val,lines=0;

	if (argc < 2) {
		help();
		return 1;
	}

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '-' || *a == '/') {
			do { a++; } while (*a == '-' || *a == '/');

			if (!strcmp(a,"dr")) {
				cmd = CMD_DUMPRAW;
			}
			else if (!strcmp(a,"d")) {
				cmd = CMD_DUMP;
			}
			else if (!strcmp(a,"ch")) {
				ch_help();
				return 1;
			}
			else if (!strncmp(a,"pi:",3)) {
				if (pi_args < PI_MAX) {
					pi_arg[pi_args++] = a+3;
				}
				else {
					fprintf(stderr,"Too many /pi: args\n");
					return 1;
				}
			}
			else {
				help();
				return 1;
			}
		}
		else {
			help();
			return 1;
		}
	}

	printf("Intel PIIX3 motherboard toy\n");
	if (cmd < 0) cmd = CMD_DUMP;

	cpu_probe();
	if (!probe_vga()) {
		printf("Cannot init VGA\n");
		return 1;
	}
	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!pci_probe(-1)) {
		printf("Cannot init library\n");
		return 1;
	}
	if (pci_bios_last_bus == -1) {
		printf("  Autodetecting PCI bus count...\n"); PAGEME;
		pci_probe_for_last_bus();
	}
	printf("  Last bus:                 %d\n",pci_bios_last_bus); PAGEME;
	printf("  Bus decode bits:          %d\n",pci_bus_decode_bits); PAGEME;

	/* then enumerate the bus */
	{
		uint8_t bus,dev,func;
		for (bus=0;bus <= pci_bios_last_bus;bus++) {
			for (dev=0;dev < 32;dev++) {
				uint8_t functions = pci_probe_device_functions(bus,dev);
				for (func=0;func < functions;func++) {
					/* make sure something is there before announcing it */
					uint16_t vendor,device,subsystem,subvendor_id;
					uint32_t class_code;
					uint8_t revision_id;

					vendor = pci_read_cfgw(bus,dev,func,0x00); if (vendor == 0xFFFF) continue;
					device = pci_read_cfgw(bus,dev,func,0x02); if (device == 0xFFFF) continue;
					subvendor_id = pci_read_cfgw(bus,dev,func,0x2C);
					subsystem = pci_read_cfgw(bus,dev,func,0x2E);
					class_code = pci_read_cfgl(bus,dev,func,0x08);
					revision_id = class_code & 0xFF;
					class_code >>= 8UL;

					if (vendor == 0x8086 && device == 0x7000 && class_code == 0x060100UL) {
						if (pci_isa_bridge.bus == 0xFF) {
							pci_isa_bridge.bus = bus;
							pci_isa_bridge.dev = dev;
							pci_isa_bridge.func = func;
							pci_isa_bridge.vendor = vendor;
							pci_isa_bridge.device = device;
						}
					}
				}
			}
		}
	}

	if (pci_isa_bridge.bus != 0xFF) {
		unsigned char b;
		unsigned long l;
		unsigned short w;

		printf("PCI-ISA bridge device: B/D/F %X:%X:%X V/D %X:%X\n",
			pci_isa_bridge.bus,	pci_isa_bridge.dev,
			pci_isa_bridge.func,	pci_isa_bridge.vendor,
			pci_isa_bridge.device); PAGEME;

		for (i=0;i < pi_args;i++) {
			const char *a = pi_arg[i];

			if (!strncmp(a,"dma-alias=",10)) {
				val = atoi(a+10);
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C);
				b = (b & 0x7F) | (((val^1)&1) << 7);
				pci_write_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C,b);
			}
			else if (!strncmp(a,"io8r=",5)) {
				val = atoi(a+5);
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C);
				if (val == 0) { /* disable */
					b &= ~(1 << 6);
					b &= ~(7 << 3);
					b |=  (1 << 3);
				}
				else { /* enable */
					b |=  (1 << 6);
					b &= ~(7 << 3);
					b |=  ((val&7) << 3);
				}
				pci_write_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C,b);
			}
			else if (!strncmp(a,"io16r=",6)) {
				val = atoi(a+6);
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C);
				if (val == 0) { /* disable */
					b &= ~(1 << 2);
					b &= ~(3 << 0);
					b |=  (1 << 0);
				}
				else { /* enable */
					b |=  (1 << 2);
					b &= ~(3 << 0);
					b |=  ((val&3) << 0);
				}
				pci_write_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C,b);
			}
			else if (!strncmp(a,"apic-cs=",8)) {
				val = atoi(a+8);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 8)) | ((val&1) << 8);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"extbios=",8)) {
				val = atoi(a+8);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 7)) | ((val&1) << 7);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"lowbios=",8)) {
				val = atoi(a+8);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 6)) | ((val&1) << 6);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"copren=",7)) {
				val = atoi(a+7);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 5)) | ((val&1) << 5);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"ps2irq=",7)) {
				val = atoi(a+7);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 4)) | ((val&1) << 4);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"bioswrite=",10)) {
				val = atoi(a+10);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 2)) | ((val&1) << 2);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"keyio=",6)) {
				val = atoi(a+6);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 1)) | ((val&1) << 1);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"rtio=",5)) {
				val = atoi(a+5);
				w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);
				w = (w & ~(1 << 0)) | ((val&1) << 0);
				pci_write_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E,w);
			}
			else if (!strncmp(a,"pci-irq-",8) && a[9] == '=') {
				int which = (tolower(a[8]) - 'a')&3;
				val = atoi(a+10);
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x60+which);
				if (val > 0) {
					b &= ~0x80;
					b |= val&15;
				}
				else {
					b &= ~15;
					b |= 0x80;
				}
				pci_write_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x60+which,b);
			}
		}

		if (cmd == CMD_DUMPRAW) {
			printf(" * RAW DUMP\n");
			printf("    ");
			for (i=0;i < 0x10;i++) {
				if ((i&0xF) == 0x8) printf(" ");
				printf("%X  ",i);
			}
			printf("\n");
			for (i=0;i < 0x100;i++) {
				unsigned char c = pci_read_cfgb(
					pci_isa_bridge.bus,
					pci_isa_bridge.dev,
					pci_isa_bridge.func,i);
				if ((i&0xF) == 0x0) printf("%02X: ",i);
				if ((i&0xF) == 0x8) printf(" ");
				printf("%02X ",c);
				if ((i&0xF) == 0xF) printf("\n");
			}
		}
		else if (cmd == CMD_DUMP) {
			/* @0x4C: IORT ISA I/O RECOVERY TIMER REGISTER (R/W)
			          DEFAULT: 0x4C

				  bit 7: DMA Reserved Page Register Aliasing Control
				         1=disable aliasing for 80h, 84h-86h, 88h,
					 and 8Ch-8Eh and forward I/O to the ISA BUS
					 0=enable aliasing

				  bit 6: 8-bit I/O Recovery Enable
				         1=Enable the recovery time in bits[5:3]
					 0=Disable recovery time, insert 3.5 SYSCLK time instead (Is that what the datasheet means?)

				  bits 5-3: 8-bit I/O Recovery Time
				            When bit 6 set, this defines the recovery time for 8-bit I/O

					    001..111 = 1 to 7 SYSCLKs respectively
					    000 = 8 SYSCLKs

				  bit 2: 16-bit I/O Recovery Enable
				         1=Enable the recovery time in bits[1:0]
					 0=Disable recovery time and (insert 3.5 SYSCLK recovery?)

				  bits 1-0: 16-bit I/O Recovery Time

				            01-11 = 1 to 3 SYSCLKs respectively
					    00 = 4 SYSCLKs */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4C);
			
			printf("ISA I/O RECOVERY TIMER:\n"); PAGEME;
			printf("   DMA Reserved Page Register Alias:      %s\n",s_yesno((b&0x80)==0)); PAGEME;
			printf("   8-Bit I/O Recovery Enable:             %s\n",s_yesno((b&0x40)!=0)); PAGEME;
			printf("   8-Bit Recovery:                        %u SYSCLKs\n",(b&0x38) == 0 ? 8 : (b>>3)&7); PAGEME;
			printf("   16-Bit I/O Recovery Enable:            %s\n",s_yesno((b&0x04)!=0)); PAGEME;
			printf("   16-Bit Recovery:                       %u SYSCLKs\n",(b&3) == 0 ? 4 : (b&3)); PAGEME;

			/* 0x4E-0x4F XBCS X-BUS CHIP SELECT REGISTER (Func 0) (R/W)
			   PIIX: 0x4E only */
			w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x4E);

			printf("XBCS X-BUS CHIP SELECT:\n"); PAGEME;
			printf("   APIC Chip Select:                      %s\n",s_yesno((w&0x100)!=0)); PAGEME;
			printf("   Extended BIOS Enable:                  %s\n",s_yesno((w&0x080)!=0)); PAGEME;
			/* ^ Whether to assert BIOSCS# for 0xFFF80000-0xFFDFFFFF if the BIOS is that large! */
			printf("   Lower BIOS Enable:                     %s\n",s_yesno((w&0x040)!=0)); PAGEME;
			/* ^ Whether to assert BIOSCS# for 0xFFFE0000-0xFFFEFFFF i.e. the lower 64KB of the traditional 128KB BIOS.
			     Also affects the legacy real-mode alias 0xE0000-0xEFFFF */
			printf("   Coprocessor Enable (FERR# -> IRQ13):   %s\n",s_yesno((w&0x020)!=0)); PAGEME;
			/* ^ Whether the coprocessor error signal causes IRQ 13 to happen */
			printf("   PS/2 IRQ Mouse on IRQ Enable:          %s\n",s_yesno((w&0x010)!=0)); PAGEME;
			/* ^ Whether to internally link IRQ 12 to the PS/2 mouse, or allow IRQ 12 by other devices */
			printf("   Enable BIOSCS# on write:               %s\n",s_yesno((w&0x004)!=0)); PAGEME;
			printf("   Keyboard I/O Enable (KBCS#+XOE#):      %s\n",s_yesno((w&0x002)!=0)); PAGEME;
			printf("   RT Clock I/O Enable (RTCCS#+XOE#):     %s\n",s_yesno((w&0x001)!=0)); PAGEME;

			/* 0x60...0x63 PIRQx ROUTING CONTROL REGISTERS (R/W)
			   Each register controls routing for PCI interrupts A, B, C, and D.

			   Bit 7: Interrupt Routing Enable (0=enable 1=disable)
			   Bits[3:0]: IRQ to route to.

			   NOTE: IRQ assignment to PCI means the corresponding ISA IRQ signal
			         is ignored. */
			for (i=0;i < 4;i++) {
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x60+i);
				printf("PCI IRQ %c ROUTING: ",i+'A');
				if (b&0x80) printf("DISABLED/ASSIGNED TO ISA");
				else printf("IRQ %u",b&15);
				printf("\n"); PAGEME;
			}

			/* 0x69: TOP OF MEMORY (Func 0) (R/W)
			   This allows the BIOS to declare where RAM is so
			   the chipset knows whether to forward ISA/DMA cycles
			   to PCI (and RAM) or not.

			   bits[7:4]: Top of memory (1-16MB) in 1MB increments.
			       0...15 represents 1-16MB respectively.
			       If you ever wondered how BIOSes of the 1995-ish
			       era are able to offer the "ISA memory hole" option in
			       Setup, this is how: just set the TOM to 15MB and
			       further regs to represent 16MB on up if any, and the
			       chipset will forward the unclaimed memory to ISA.
			   bit 3: ISA/DMA lower BIOS forwarding enable.
			          Whether ISA or DMA access to BIOS regions is
				  forwarded to the PCI bus and chipset
			   bit 2: A,B Segment forward enable.
			          Whether ISA/DMA access to 0xA0000-0xBFFFF
				  is forwarded to PCI bus and chipset
			   bit 1: ISA 512-640KB region forward enable.
			          Whether ISA/DMA access to the 512KB-640KB region
				  is forwarded to PCI bus and chipset */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x69);

/* TODO: /pi:xxxx=N options for these values */
			printf("TOP OF MEMORY:\n"); PAGEME;
			printf("   Top of (lower 16MB) memory:            %uMB\n",(b>>4)+1); PAGEME;
			printf("   ISA/DMA Lower BIOS forward to PCI:     %s\n",s_yesno((b&0x08)!=0)); PAGEME;
			printf("   ISA/DMA A0000-BFFFF forward to PCI:    %s\n",s_yesno((b&0x04)!=0)); PAGEME;
			printf("   ISA/DMA 512-640KB forward to PCI:      %s\n",s_yesno((b&0x02)!=0)); PAGEME;

			/* 0x6A-0x6B: MSTAT MISCELLANEOUS STATUS REGISTER
			    bit 15: SERR# Generation Due to Delayed Transaction (PIIX3).
			            Clear by writing a 1 to this.
			    bit 7: NB Retry Enable (PIIX3)
			    bit 6: EXTSMI# Mode Enable (PIIX3)
			    bit 4: USB Enable (PIIX3)
			    bit 2: PCI Header Type Bit Enable (PIIX)
			    bit 1: Internal ISA DMA or External DMA Mode Status (PIIX)
			    bit 0: ISA Clock Divisor (R/O on PIIX)
			           1=divide by 3   0=divide by 4 */
			w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x6A);

/* TODO: /pi:xxxx=N options for these values */
			printf("MISC STATUS REGISTER:\n"); PAGEME;
			printf("   SERR# Generation/Delayed Transact Err: %s\n",s_yesno((w&0x8000)!=0)); PAGEME;
			printf("   NB Retry Enable:                       %s\n",s_yesno((w&0x0080)!=0)); PAGEME;
			printf("   EXTSMI# Mode Enable:                   %s\n",s_yesno((w&0x0040)!=0)); PAGEME;
			printf("   USB Enable (PIIX3):                    %s\n",s_yesno((w&0x0010)!=0)); PAGEME;
			printf("   PCI Header Type Bit Enable (PIIX):     %s\n",s_yesno((w&0x0004)!=0)); PAGEME;
			printf("   Internal ISA DMA/Ext DMA Mode Status:  %s\n",s_yesno((w&0x0002)!=0)); PAGEME;
			printf("   PCI clock divider -> ISA clock:        Divide by %u\n",(w&1)?3:4); PAGEME;

			/* 0x70: MOTHERBOARD DEVICE IRQ ROUTE CONTROL (R/W) */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x70);

/* TODO: /pi:xxxx=N options for these values */
			printf("MOTHERBOARD DEVICE IRQ ROUTE (MBIRQ0):\n"); PAGEME;
			printf("   Interrupt Routine Enable:              %s\n",s_yesno((b&0x80)!=0)); PAGEME;
			printf("   MIRQ/IRQ sharing enable:               %s\n",s_yesno((b&0x40)!=0)); PAGEME;
			printf("   PIIX3 IRQ0 Enable:                     %s\n",s_yesno((b&0x20)!=0)); PAGEME;
			printf("   Routed to:                             IRQ %u\n",b&0xF); PAGEME;

			/* 0x76, 0x77: MOTHERBOARD DEVICE DMA CONTROL (R/W) */
			for (i=0;i < 2;i++) {
				b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x76+i);

/* TODO: /pi:xxxx=N options for these values */
				printf("MOTHERBOARD DEVICE DMA CONTROL (MBDMA%u#):\n",i); PAGEME;
				printf("   Type F and DMA Buffer Enable:          %s\n",s_yesno((b&0x80)!=0)); PAGEME;
				printf("   Channel:                               ");
				if ((b&7) == 4) printf("DISABLED");
				else printf("%u",b&7);
				printf("\n"); PAGEME;
			}

			/* 0x78-0x79: PROGRAMMABLE CHIP SELECT CONTROL
			   If enabled, I/O access to this range triggers the PCS#
			   signal which motherboard manufacturers can then use for
			   whatever purpose. */
			w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x78);

			printf("PROGRAMMABLE CHIP SELECT:\n"); PAGEME;
			if ((w & 3) == 2) {
				printf("   DISABLED\n");
			}
			else {
				printf("   Base address:                          0x%03X\n",w & 0xFFFC); PAGEME;
				printf("   Range:                                 %u ports\n",
					(w&3)==3 ? 16 : (4 << (w&3)));
			}

			/* 0x80: APIC BASE ADDRESS RELOCATION REGISTER (PIIX3) */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x80);

			printf("APIC BASE ADDRESS:\n"); PAGEME;
			printf("   A12 mask:                              %s\n",s_yesno((b&0x40)!=0)); PAGEME;
			printf("   X AD[15:12]:                           %X\n",(b>>2)&0xF); PAGEME;
			printf("   Y AD[11:10]:                           %X\n",b&3); PAGEME;

			/* 0x82: DETERMINISTIC LATENCY CONTROL (PIIX3) */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0x82);

			printf("DETERMINISTIC LATENCY CONTROL:\n"); PAGEME;
			printf("   SERR# Generation/Delayed Trans. en:    %s\n",s_yesno((b&0x08)!=0)); PAGEME;
			printf("   USB Passive Release:                   %s\n",s_yesno((b&0x04)!=0)); PAGEME;
			printf("   Passive Release enable:                %s\n",s_yesno((b&0x02)!=0)); PAGEME;
			printf("   Delayed Transaction enable:            %s\n",s_yesno((b&0x01)!=0)); PAGEME;

			/* 0xA0: SMI CONTROL REGISTER */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xA0);

			printf("SMI CONTROL REGISTER:\n"); PAGEME;
			printf("   Fast Off Timer Freeze:                 ");
			if ((b&0x18) == 0x08)
				printf("DISABLED");
			else if ((b&0x18) == 0x10)
				printf("1 PCICLK");
			else
				printf("1/1.1/1.32 %s",(b&0x18)==0x18?"msec":"min");
			printf("\n"); PAGEME;
			printf("      ...(with PCI clock @ 33/30/25MHz)\n"); PAGEME;
			printf("   STPCLK# Scale enable:                  %s\n",s_yesno((b&0x04)!=0)); PAGEME;
			printf("   STPCLK# Signal enable:                 %s\n",s_yesno((b&0x02)!=0)); PAGEME;
			printf("   SMI# Gate:                             %s\n",s_yesno((b&0x01)!=0)); PAGEME;

			/* 0xA2-0xA3: SMI ENABLE REGISTER */
			w = pci_read_cfgw(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xA2);

			printf("SMI ENABLE REGISTER:\n"); PAGEME;
			printf("   Legacy USB SMI Enable:                 %s\n",s_yesno((w&0x100)!=0)); PAGEME;
			printf("   APMC Write SMI Enable:                 %s\n",s_yesno((w&0x080)!=0)); PAGEME;
			printf("   EXTSMI# SMI Enable:                    %s\n",s_yesno((w&0x040)!=0)); PAGEME;
			printf("   Fast Off Timer SMI Enable:             %s\n",s_yesno((w&0x020)!=0)); PAGEME;
			printf("   IRQ 12 SMI Enable (PS/2 Mouse):        %s\n",s_yesno((w&0x010)!=0)); PAGEME;
			printf("   IRQ 8 SMI Enable (RTC Alarm):          %s\n",s_yesno((w&0x008)!=0)); PAGEME;
			printf("   IRQ 4 SMI Enable (COM2/COM4):          %s\n",s_yesno((w&0x004)!=0)); PAGEME;
			printf("   IRQ 3 SMI Enable (COM1/COM3):          %s\n",s_yesno((w&0x002)!=0)); PAGEME;
			printf("   IRQ 1 SMI Enable (Keyboard):           %s\n",s_yesno((w&0x001)!=0)); PAGEME;

			/* 0xA4-0xA7: SYSTEM EVENT ENABLE */
			l = pci_read_cfgl(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xA4);

			printf("SYSTEM EVENT ENABLE:\n"); PAGEME;
			printf("   Fast Off SMI Enable:                   %s\n",s_yesno((l&0x80000000UL)!=0UL)); PAGEME;
			printf("   INTR Enable:                           %s\n",s_yesno((l&0x40000000UL)!=0UL)); PAGEME;
			printf("   Fast Off NMI Enable:                   %s\n",s_yesno((l&0x20000000UL)!=0UL)); PAGEME;
			printf("   Fast Off APIC Enable:                  %s\n",s_yesno((l&0x10000000UL)!=0UL)); PAGEME;
			printf("   Fast Off for IRQ:                      ");
			for (i=0;i < 16;i++) {
				if (i == 2) continue;
				if (l & (1UL << ((unsigned long)i) )) printf("%u ",i);
			}
			printf("\n"); PAGEME;

			/* 0xA8: FAST OFF TIMER REGISTER */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xA8);

			printf("FAST OFF TIMER: %u\n",(unsigned int)b + 1U); PAGEME;

			/* 0xAC: CLOCK SCALE STPCLK# LOW TIMER */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xAC);

			printf("CLOCK SCALE STPCLK# LOW TIMER: %u\n",b); PAGEME;

			/* 0xAE: CLOCK SCALE STPCLK# HIGH TIMER */
			b = pci_read_cfgb(pci_isa_bridge.bus,pci_isa_bridge.dev,pci_isa_bridge.func,0xAE);

			printf("CLOCK SCALE STPCLK# HIGH TIMER: %u\n",b); PAGEME;
		}
	}

	return 0;
}
