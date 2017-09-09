
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

/* A word of caution: There do exist late 1990's motherboard chipsets with bugs
 * in the PCI controller regarding byte-size access. On a Pentium III Celeron Dell
 * Dimension, both the BIOS and PCI controller (port 0xCF8-0xCFF) read/write the
 * wrong byte in configuration space when accessing bytewise. If your code must
 * work on such systems, try to use word/dword configuration space access where
 * possible. */

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
unsigned char           pci_cfg_presence_filtering = 0; /* if enabled, we refuse I/O to PCI bus/device combinations we know
                                                           don't exist as a concession to shitty PCI bus implementations */
uint32_t			pci_bios_protmode_entry_point = 0;
uint8_t				pci_bios_hw_characteristics = 0;
uint16_t			pci_bios_interface_level = 0;
uint8_t				pci_bus_decode_bits = 0;	/* which bus bits the hardware actually bothers to compare against */
int16_t				pci_bios_last_bus = 0;

#ifndef TARGET_PC98
/* external assembly language functions */
int				try_pci_bios2();
int				try_pci_bios1();
# if TARGET_MSDOS == 16
uint32_t __cdecl		pci_bios_read_dword_16(uint16_t bx,uint16_t di);
void __cdecl			pci_bios_write_dword_16(uint16_t bx,uint16_t di,uint32_t data);
# endif
#endif

/* NTS: Programming experience tells me that depite what this bitfield arrangement
 *      suggests, most PCI devices ignore bits 0-1 of the register number and expect
 *      you to offset the read from the 0xCFC register instead. */
unsigned char pci_type1_refuse = 0;
void pci_type1_select(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg) {
	outpd(0xCF8,0x80000000UL |
		(((uint32_t)bus)  << 16UL) |
		(((uint32_t)card) << 11UL) |
		(((uint32_t)func) <<  8UL) |
		 ((uint32_t)(reg & 0xFC)));
}

#ifndef TARGET_PC98
void pci_type2_select(uint8_t bus,uint8_t func) {
	/* FIXME: Is this right? Documentation is sparse and hard to come by... */
	outp(0xCF8,0x80 | (func << 1));
	outp(0xCFA,bus);
}
#endif

uint32_t pci_read_cfg_TYPE1(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
	pci_type1_select(bus,card,func,reg);
	switch (size) {
		case 2:	return inpd(0xCFC);
		case 1:	return inpw(0xCFC+(reg&3));
		case 0: return inp(0xCFC+(reg&3));
	}

	return ~0UL;
}

#ifdef TARGET_PC98
uint32_t pci_read_cfg_TYPE1_pc(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) {
    /* PCI bus handling on NEC-PC9821 requires some additional work.
     * The systems I've tested on so far appear to let the bus float instead of returning 0xFFFFFFFF
     * for undefined devices. Some additional filtering is required to weed out valid devices from
     * undefined devices. Since this can cause overhead, we only do this for the Vendor and Device
     * fields of the PCI configuration space. */
    uint32_t r = pci_read_cfg_TYPE1(bus,card,func,reg,size);

    if (pci_cfg_presence_filtering) {
        if (reg == 0 && size >= 2) { /* within device ID or vendor ID fields */
            uint32_t ors[0x40U>>2U],ands[0x40U>>2U];
            unsigned int i,j;
            uint32_t r2;

            /* holy shit the NEC PC-9821 has the worst PCI chipset I've ever written code for.
             * the only excuse I can think of is that this is early 486 PCI chipset crap and
             * that the drivers written for Windows 3.1 probably whitelisted the device IDs
             * to avoid trouble. Blegh. I can only imagine the random surprises that would
             * result when the end user upgraded to Windows 95.
             *
             * here's what we do to detect:
             *
             * repeatedly read the configuration space by DWORD from 0x00 to 0x3F.
             * for each value we read:
             *
             *   ors[reg>>2] |= r2
             *   ands[reg>>2] &= r2
             *
             * if the values are consistent, ors[i] == ands[i].
             * else, the random garbage will give different values. */
            for (j=0;j < (0x40U>>2U);j++) {
                ands[j] = 0xFFFFFFFFUL;
                ors[j] = 0;
            }

            /* do it 512 times to be absolutely sure.
             * 128 times is not enough.
             * 256 times is not enough.
             * anything less than 512 times and when we enumerate past all valid devices,
             * the garbage becomes consistent enough to make false positives.
             *
             * anyone who complains that this makes PCI enumeration too slow should understand
             * this is the only way to get a proper enumeration of the PCI bus without garbage
             * devices showing up.
             *
             * this is the price to pay when the difference between a PCI device and nothing
             * is random and unpredictable data. Yuck. Blegh. Barf.
             *
             * PCI devices don't normally change device ID, vendor ID, status, command, cache
             * lines, BARs, or anything else in the first 0x40 while reading. If they do,
             * in an environment like MS-DOS where nothing usually changes that, then there's
             * something wrong with the PCI device and it deserves to be ignored. >:[ */
            for (j=0;j < 512;j++) {
                if ((j&3U) == 0U) {
                    /* prime the PCI bus with a known working device and register.
                     * not doing this means that when the bus floats as high as it goes,
                     * the test below will give false positives because the data is consistent again. */
                    pci_read_cfg_TYPE1(/*bus*/0,/*card*/0,/*func*/0,/*reg*/j&0x3FU,2/*DWORD*/);
                }

                for (i=0;i < 0x40;i += 4) {
                    r2 = pci_read_cfg_TYPE1(bus,card,func,i,2/*DWORD*/);
                    ors[i>>2U] |= r2;
                    ands[i>>2U] &= r2;
                }
            }

            /* is it consistent? */
            for (j=0;j < (0x40U>>2U);j++) {
                if (ors[j] != ands[j])
                    goto filter_out;
            }
        }
    }

    return r;
filter_out:
    return (uint32_t)0xFFFFFFFFUL;
}
#endif

#ifndef TARGET_PC98
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
#endif

#ifndef TARGET_PC98
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
#endif

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

#ifndef TARGET_PC98
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
#endif

#ifndef TARGET_PC98
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
#endif

void pci_write_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) {
}

void pci_probe_for_last_bus() {
	/* scan backwards until we find a root PCI device that doesn't return 0xFFFF for ID.
	 * but also keep track of the IDs because some PCI busses (especially ancient Pentium
	 * based laptops) don't bother decoding the bus field. If we're not careful, we could
	 * erroneously come up with 16 busses attached, each having the same device list 16
	 * times */
	uint32_t id[16],bar[16],sub[16];
	uint8_t bus,card;

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

	/* now check for the last working root device, and mark that. */
	/* NTS: I have a Dell Optiplex Pentium III Celeron system where motherboard devices
	 *      enumerate as PCI devices on bus 0, and PCI cards enumerate on bus 1, but there
	 *      is no device on bus=1 card=0 function=0. if we only counted busses where there
	 *      was a device 0 then this code would miss all PCI add-in cards on that system.
	 *
	 *      At the time I found this, I had also noticed that the 3Dfx glide drivers for
	 *      DOS would fail to find the Voodoo II card in the system even though the card
	 *      was plugged in and fully functional. Perhaps GLIDE2.OVL was making the same
	 *      assumption about PCI device enumeration.
	 *
	 *      So to work with these systems, we check for any devices on that bus rather
	 *      than assume no devices if no device 0. */
	for (bus=0xF;bus != 0;) {
		/* if we already know there's something there, then stop */
		if (id[bus] != ~0UL) break;

#ifdef TARGET_PC98
        /* if we're working with shit PC-9821 PCI bus hardware it's highly unlikely
         * there's anything past bus 1, let alone bus 0. probing is slowed down as
         * it is trying to differentiate between real devices and garbage on the PCI
         * bus. don't waste our time. */
        if (pci_cfg_presence_filtering && bus > 1) {
            bus--;
            continue;
        }
#endif

		/* nothing found there. let's scan */
		for (card=0;card < 32;card++) {
			id[bus] = pci_read_cfgl(bus,card,0,0x00);
			if (id[bus] != ~0UL) break;
		}

		if (id[bus] == ~0UL)
			bus--; // nothing found. keep scanning.
		else
			break; // found something. stop.
	}
	pci_bios_last_bus = bus;
}

int try_pci_type2() {
	int ret = 0;
#ifndef TARGET_PC98
	outp(0xCF8,0);
	outp(0xCFA,0);
	if (inp(0xCF8) == 0 && inp(0xCFA) == 0) ret = 1;
#endif
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
#ifndef TARGET_PC98
		case PCI_CFG_BIOS:
			if (try_pci_bios2()) pci_cfg = PCI_CFG_BIOS;
			break;
		case PCI_CFG_TYPE2:
			if (try_pci_type2()) pci_cfg = PCI_CFG_TYPE2;
			break;
		case PCI_CFG_BIOS1:
			if (try_pci_bios1()) pci_cfg = PCI_CFG_BIOS1;
			break;
#endif
		default:
			if (pci_cfg == PCI_CFG_NONE) { /* Type 1? This is most common, and widely supported */
				if (try_pci_type1()) pci_cfg = PCI_CFG_TYPE1;
			}
#ifndef TARGET_PC98
			if (pci_cfg == PCI_CFG_NONE) {
				if (try_pci_bios2()) pci_cfg = PCI_CFG_BIOS;
			}
			if (pci_cfg == PCI_CFG_NONE) { /* Type 2? This is rare, I have no hardware that supports this... */
				if (try_pci_type2()) pci_cfg = PCI_CFG_TYPE2;
			}
			if (pci_cfg == PCI_CFG_NONE) {
				if (try_pci_bios1()) pci_cfg = PCI_CFG_BIOS1;
			}
#endif
			break;
	}

#ifdef TARGET_PC98
    /* Presence filtering for NEC PC-9821: Their PCI bus implementation isn't so good when it comes
     * to undefined devices, at least on 486 systems. Rather than do what Intel systems do, and
     * return 0xFFFFFFFF, they let the bus float and read back whatever random value it floats to.
     * Undefined vendor IDs either read back 0x0000 or random values up to 0xFFE1. To prevent the
     * host application from enumerating junk devices, we need to enable presence filtering and
     * return 0xFFFF for these devices. */
    pci_cfg_presence_filtering = 1;
#endif

	pci_cfg_probed = 1;
	return pci_cfg;
}

#ifdef TARGET_PC98
uint32_t (*pci_read_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) = {
	pci_read_cfg_NOTIMPL,		/* NONE */
	pci_read_cfg_TYPE1_pc,		/* TYPE1 */
	pci_read_cfg_NOTIMPL,		/* BIOS */
	pci_read_cfg_NOTIMPL,		/* BIOS1 */
	pci_read_cfg_NOTIMPL		/* TYPE2 */
};
#else
uint32_t (*pci_read_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint8_t size) = {
	pci_read_cfg_NOTIMPL,		/* NONE */
	pci_read_cfg_TYPE1,		/* TYPE1 */
	pci_read_cfg_BIOS,		/* BIOS */
	pci_read_cfg_NOTIMPL,		/* BIOS1 */
	pci_read_cfg_TYPE2		/* TYPE2 */
};
#endif

#ifdef TARGET_PC98
void (*pci_write_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) = {
	pci_write_cfg_NOTIMPL,		/* NONE */
	pci_write_cfg_TYPE1,		/* TYPE1 */
	pci_write_cfg_NOTIMPL,		/* BIOS */
	pci_write_cfg_NOTIMPL,		/* BIOS1 */
	pci_write_cfg_NOTIMPL		/* TYPE2 */
};
#else
void (*pci_write_cfg_array[PCI_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint8_t reg,uint32_t data,uint8_t size) = {
	pci_write_cfg_NOTIMPL,		/* NONE */
	pci_write_cfg_TYPE1,		/* TYPE1 */
	pci_write_cfg_BIOS,		/* BIOS */
	pci_write_cfg_NOTIMPL,		/* BIOS1 */
	pci_write_cfg_TYPE2		/* TYPE2 */
};
#endif

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

