
/* NTS: It's not required in the DOS environment, but in the unlikely event that
 *      interrupt handlers are mucking around with PCI configuration space you
 *      may want to bracket your PCI operations with CLI/STI. Again for performance
 *      reasons, this code will NOT do it for you */
/* NTS: Additional research suggests that on legacy systems with 10-bit decoding
 *      I/O ports 0xCF8-0xCFF are unlikely to collide with 287/387 legacy I/O
 *      ports 0xF0-0xF7 (as I initially thought), therefore probing 0xCF8 on
 *      ancient systems should do no harm and not cause any problems. */

/* This library enables programming the PCI bus that may be available to DOS level
 * programs on Pentium and higher systems (where PCI slots were first common on
 * home desktop PCs)
 *
 * This code is designed to compile 16-bit and 32-bit versions. The 16-bit real
 * mode versions are designed with DOS programs in mind that might want the ability
 * to fiddle with the PCI bus and program registers while maintaining downlevel
 * compatability with older hardware, perhaps all the way down to the 8086. */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/pci/pci.h>
#include <hw/cpu/cpu.h>

unsigned char			pci_cfg_probed = 0;
unsigned char			pci_cfg = PCI_CFG_NONE;
uint32_t			pci_bios_protmode_entry_point = 0;
uint8_t				pci_bios_hw_characteristics = 0;
uint16_t			pci_bios_interface_level = 0;
uint8_t				pci_bus_decode_bits = 0;	/* which bus bits the hardware actually bothers to compare against */
int16_t				pci_bios_last_bus = 0;

/* external assembly language functions */
int				try_pci_bios2();
int				try_pci_bios1();
#if TARGET_MSDOS == 16
uint32_t __cdecl		pci_bios_read_dword_16(uint16_t bx,uint16_t di);
void __cdecl			pci_bios_write_dword_16(uint16_t bx,uint16_t di,uint32_t data);
#endif

/* NTS: Programming experience tells me that depite what this bitfield arrangement
 *      suggests, most PCI devices ignore bits 0-1 of the register number and expect
 *      you to offset the read from the 0xCFC register instead. */
void pci_type1_select(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg) {
	outpd(0xCF8,0x80000000UL |
		(((uint32_t)bus)  << 16UL) |
		(((uint32_t)card) << 11UL) |
		(((uint32_t)func) <<  8UL) |
		 ((uint32_t)(reg & 0xFC)));
}

void pci_type2_select(uint8_t bus,uint8_t func) {
	/* FIXME: Is this right? Documentation is sparse and hard to come by... */
	outp(0xCF8,0x80 | (func << 1));
	outp(0xCFA,bus);
}

uint32_t pci_read_cfg_TYPE1(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
	pci_type1_select(bus,card,func,reg);
	switch (size) {
		case 2:	return inpd(0xCFC);
		case 1:	return inpw(0xCFC+(reg&3));
		case 0: return inp(0xCFC+(reg&3));
	}

	return ~0UL;
}

/* WARNING: I have no hardware to verify this works */
uint32_t pci_read_cfg_TYPE2(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
	const uint16_t pt = 0xC000 + (card << 8) + reg;
	pci_type2_select(bus,func);
	switch (size) {
		case 2:	return inpd(pt);
		case 1:	return inpw(pt);
		case 0: return inp(pt);
	}

	return ~0UL;
}

uint32_t pci_read_cfg_BIOS(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
	static const uint32_t msks[3] = {0xFFUL,0xFFFFUL,0xFFFFFFFFUL};
	union REGS regs;
	
	if (size > 2) return ~0UL;
#if TARGET_MSDOS == 16
	if (size == 2) { /* 32-bit DWORD read in real mode when Watcom won't let me use 386 style registers or int386() */
		return pci_bios_read_dword_16(
			/* BH=bus BL(7-3)=card BL(2-0)=func */
			(bus << 8) | (card << 3) | func,
			/* DI=reg */
			reg);
		return ~0UL;
	}
#endif

	regs.w.ax = 0xB108 + size;
	regs.w.bx = (bus << 8) | (card << 3) | func;
	regs.w.di = reg;
#if TARGET_MSDOS == 32
	int386(0x1A,&regs,&regs);
#else
	int86(0x1A,&regs,&regs);
#endif
	if (regs.w.cflag & 1) /* carry flag set on error */
		return ~0UL;
	if (regs.h.ah != 0x00) /* AH=0x00 if success */
		return ~0UL;

#if TARGET_MSDOS == 32
	return regs.x.ecx & msks[size];
#else
	return regs.w.cx & msks[size];
#endif
}

uint32_t pci_read_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
	return ~0UL;
}

void pci_write_cfg_TYPE1(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) {
	pci_type1_select(bus,card,func,reg);
	switch (size) {
		case 2:	outpd(0xCFC,data);
		case 1:	outpw(0xCFC+(reg&3),data);
		case 0: outp(0xCFC+(reg&3),data);
	}
}

/* WARNING: I have no hardware to verify this works */
void pci_write_cfg_TYPE2(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) {
	const uint16_t pt = 0xC000 + (card << 8) + reg;
	pci_type2_select(bus,func);
	switch (size) {
		case 2:	outpd(pt,data);
		case 1:	outpw(pt,data);
		case 0: outp(pt,data);
	}
}

void pci_write_cfg_BIOS(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) {
	union REGS regs;
	
	if (size > 2) return;
#if TARGET_MSDOS == 16
	if (size == 2) { /* 32-bit DWORD read in real mode when Watcom won't let me use 386 style registers or int386() */
		pci_bios_write_dword_16(
			/* BH=bus BL(7-3)=card BL(2-0)=func */
			(bus << 8) | (card << 3) | func,
			/* DI=reg */
			reg,
			/* data */
			data);
		return;
	}
#endif

	regs.w.ax = 0xB10B + size;
	regs.w.bx = (bus << 8) | (card << 3) | func;
	regs.w.di = reg;
#if TARGET_MSDOS == 32
	regs.x.ecx = data;
	int386(0x1A,&regs,&regs);
#else
	regs.w.cx = (uint16_t)data;
	int86(0x1A,&regs,&regs);
#endif
}

void pci_write_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) {
}

void pci_probe_for_last_bus() {
	/* scan backwards until we find a root PCI device that doesn't return 0xFFFF for ID.
	 * but also keep track of the IDs because some PCI busses (especially ancient Pentium
	 * based laptops) don't bother decoding the bus field. If we're not careful, we could
	 * erroneously come up with 16 busses attached, each having the same device list 16
	 * times */
	uint32_t id[16],bar[16],sub[16];
	uint8_t bus;

	for (bus=0;bus < 16;bus++) {
		id[bus] = pci_read_cfgl(bus,0,0,0x00);
		bar[bus] = pci_read_cfgl(bus,0,0,0x10);
		sub[bus] = pci_read_cfgl(bus,0,0,0x2C);
	}

	/* check for cheap PCI busses that don't decode the BUS field of the PCI configuration register */
	bus = 3;
	pci_bus_decode_bits = bus + 1;
	do {
		uint8_t pm = 1 << bus;
		if (id[pm] == id[0] && bar[pm] == bar[0] && sub[pm] == sub[0]) {
			uint8_t ii;

			/* looks like a mirror image to me. rub it out */
			pci_bus_decode_bits = bus;
			for (ii=pm;ii < (2 << bus);ii++) {
				id[ii] = bar[ii] = sub[ii] = ~0UL;
			}
		}
		else {
			/* not a match, so it's probably not wise to check further */
			break;
		}
	} while (bus-- != 0);

	/* now check for the last working root device, and mark that */
	for (bus=0xF;bus != 0 && id[bus] == ~0UL;) bus--;
	pci_bios_last_bus = bus;
}

int try_pci_type2() {
	int ret = 0;
	outp(0xCF8,0);
	outp(0xCFA,0);
	if (inp(0xCF8) == 0 && inp(0xCFA) == 0) ret = 1;
	return ret;
}

int try_pci_type1() {
	int ret = 0;
	uint32_t tmp = inpd(0xCF8);
	outpd(0xCF8,0x80000000);
	if (inpd(0xCF8) == 0x80000000 && inpd(0xCFC) != 0xFFFFFFFFUL) ret=1; /* might be type 2 reflecting written data */
	outpd(0xCF8,tmp);
	return ret;
}

/* WARNING: Uses 32-bit I/O. The caller is expected to have first called the CPU library to determine we are a 386 or higher */
int pci_probe(int preference) {
	if (pci_cfg_probed)
		return pci_cfg;

#if TARGET_MSDOS == 16
	/* This code won't even run on pre-386 if compiled 32-bit, so this check is limited only to 16-bit real mode builds that might be run on such hardware */
	if (cpu_basic_level < 3) /* NTS: I'm aware this variable is initialized to ~0 (255) first */
		return PCI_CFG_NONE; /* if the programmer is not checking the CPU he either doesn't care about pre-386 systems or he is stupid, so go ahead and do it anyway */
#endif

	pci_bios_protmode_entry_point = 0;
	pci_bios_hw_characteristics = 0;
	pci_bios_interface_level = 0;
	pci_cfg = PCI_CFG_NONE;
	pci_bios_last_bus = -1;	/* -1 means "I don't know" */
	pci_bus_decode_bits = 4;

	switch (preference) {
		case PCI_CFG_TYPE1:
			if (try_pci_type1()) pci_cfg = PCI_CFG_TYPE1;
			break;
		case PCI_CFG_BIOS:
			if (try_pci_bios2()) pci_cfg = PCI_CFG_BIOS;
			break;
		case PCI_CFG_TYPE2:
			if (try_pci_type2()) pci_cfg = PCI_CFG_TYPE2;
			break;
		case PCI_CFG_BIOS1:
			if (try_pci_bios1()) pci_cfg = PCI_CFG_BIOS1;
			break;
		default:
			if (pci_cfg == PCI_CFG_NONE) { /* Type 1? This is most common, and widely supported */
				if (try_pci_type1()) pci_cfg = PCI_CFG_TYPE1;
			}
			if (pci_cfg == PCI_CFG_NONE) {
				if (try_pci_bios2()) pci_cfg = PCI_CFG_BIOS;
			}
			if (pci_cfg == PCI_CFG_NONE) { /* Type 2? This is rare, I have no hardware that supports this... */
				if (try_pci_type2()) pci_cfg = PCI_CFG_TYPE2;
			}
			if (pci_cfg == PCI_CFG_NONE) {
				if (try_pci_bios1()) pci_cfg = PCI_CFG_BIOS1;
			}
			break;
	}

	pci_cfg_probed = 1;
	return pci_cfg;
}

uint32_t (*pci_read_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) = {
	pci_read_cfg_NOTIMPL,		/* NONE */
	pci_read_cfg_TYPE1,		/* TYPE1 */
	pci_read_cfg_BIOS,		/* BIOS */
	pci_read_cfg_NOTIMPL,		/* BIOS1 */
	pci_read_cfg_TYPE2		/* TYPE2 */
};

void (*pci_write_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) = {
	pci_write_cfg_NOTIMPL,		/* NONE */
	pci_write_cfg_TYPE1,		/* TYPE1 */
	pci_write_cfg_BIOS,		/* BIOS */
	pci_write_cfg_NOTIMPL,		/* BIOS1 */
	pci_write_cfg_TYPE2		/* TYPE2 */
};

uint8_t pci_probe_device_functions(uint8_t bus,uint8_t dev) {
	uint8_t funcs=8,f;
	uint32_t id[8],bar[8],sub[8];

	/* some laptop PCI chipsets, believe it or not, decode bus and device numbers but ignore the function index.
	 * if we're not careful, we could end up with the misconception that there were 8 of each PCI device present. */

	for (f=0;f < 8;f++) {
		id[f] = pci_read_cfgl(bus,dev,f,0x00);
		bar[f] = pci_read_cfgl(bus,dev,f,0x10);
		sub[f] = pci_read_cfgl(bus,dev,f,0x2C);
		if (f == 0 && id[f] == 0xFFFFFFFFUL) return 0;
	}

	while (funcs > 1) {
		f = funcs >> 1;
		if (id[0] == id[f] && bar[0] == bar[f] && sub[0] == sub[f])
			funcs = f;
		else
			break;
	}

	return funcs;
}

