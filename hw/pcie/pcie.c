
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

/* TODO: Need 64-bit I/O functions (using Pentium MMX MOVQ) for 16-bit flat real mode */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/flatreal/flatreal.h>
#include <hw/llmem/llmem.h>
#include <hw/acpi/acpi.h>	/* on x86 systems you need the ACPI BIOS to locate PCI Express MMIO space */
#include <hw/pcie/pcie.h>
#include <hw/cpu/cpu.h>
#include <hw/dos/doswin.h>

unsigned char			pcie_cfg_probed = 0;
unsigned char			pcie_cfg = PCIE_CFG_NONE;
uint8_t				pcie_bus_decode_bits = 0;	/* which bus bits the hardware actually bothers to compare against */
uint8_t				pcie_bios_last_bus = 0;
uint64_t			pcie_acpi_mcfg_table = 0;
uint64_t			pcie_bus_mmio_base[256]={0ULL};

uint32_t pcie_read_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint8_t size) {
	return ~0UL;
}

uint32_t pcie_read_cfg_FLATREAL(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint8_t size) {
#if TARGET_MSDOS == 16
	/* we're in 16-bit real mode and we're using the Flat Real Mode library */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];

		if (!flatrealmode_allowed())
			return ~0UL;
		if (!flatrealmode_ok()) {
			if (!flatrealmode_setup(FLATREALMODE_4GB))
				return ~0UL;
		}

		switch (size) {
			case 0:	return flatrealmode_readb(addr);
			case 1:	return flatrealmode_readw(addr);
			case 2:	return flatrealmode_readd(addr);
		};
	}
#endif
	return ~0UL;
}

uint32_t pcie_read_cfg_PTR(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint8_t size) {
#if TARGET_MSDOS == 32
	/* we're in flat 32-bit protected mode and the DOS extender left off paging */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];
		switch (size) {
			case 0:	return *((uint8_t*)addr);
			case 1:	return *((uint16_t*)addr);
			case 2:	return *((uint32_t*)addr);
		};
	}
#endif
	return ~0UL;
}

uint64_t pcie_read_cfg_NOTIMPLq(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg) {
	return ~0ULL;
}

uint64_t pcie_read_cfg_PTRq(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg) {
#if TARGET_MSDOS == 32
	/* we're in flat 32-bit protected mode and the DOS extender left off paging */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		/* use MOVQ to guarantee 64-bit memory I/O. Hope your CPU is a Pentium MMX or higher! */
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];
		uint64_t retv = 0;
		__asm {
			.686p
			.mmx
			push	esi
			mov	esi,addr
			movq	mm0,[esi]
			movq	retv,mm0
			emms
			pop	esi
		}

		return retv;
	}
#endif
	return ~0ULL;
}

void pcie_write_cfg_NOTIMPL(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint32_t data,uint8_t size) {
}

void pcie_write_cfg_FLATREAL(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint32_t data,uint8_t size) {
#if TARGET_MSDOS == 16
	/* we're in 16-bit real mode and we're using the Flat Real Mode library */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];

		if (!flatrealmode_allowed())
			return;
		if (!flatrealmode_ok()) {
			if (!flatrealmode_setup(FLATREALMODE_4GB))
				return;
		}

		switch (size) {
			case 0:	flatrealmode_writeb(addr,(uint8_t)data); return;
			case 1:	flatrealmode_writew(addr,(uint16_t)data); return;
			case 2:	flatrealmode_writed(addr,(uint32_t)data); return;
		};
	}
#endif
}

void pcie_write_cfg_PTR(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint32_t data,uint8_t size) {
#if TARGET_MSDOS == 32
	/* we're in flat 32-bit protected mode and the DOS extender left off paging */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];
		switch (size) {
			case 0:	*((uint8_t*)addr) = (uint8_t)data; return;
			case 1:	*((uint16_t*)addr) = (uint16_t)data; return;
			case 2:	*((uint32_t*)addr) = (uint32_t)data; return;
		};
	}
#endif
}

void pcie_write_cfg_NOTIMPLq(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint64_t data) {
}

void pcie_write_cfg_PTRq(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint64_t data) {
#if TARGET_MSDOS == 32
	/* we're in flat 32-bit protected mode and the DOS extender left off paging */
	if (pcie_bus_mmio_base[bus] != 0ULL) {
		/* use MOVQ to guarantee 64-bit memory I/O. Hope your CPU is a Pentium MMX or higher! */
		uint32_t addr = pci_express_bdfero_to_offset(0,card,func,reg>>2,reg&3) + (uint32_t)pcie_bus_mmio_base[bus];
		__asm {
			.686p
			.mmx
			push	esi
			mov	esi,addr
			movq	mm0,data
			movq	[esi],mm0
			emms
			pop	esi
		}
	}
#endif
}

void pcie_probe_for_last_bus() {
	/* FIXME: This probe function was needed in the PCI library because I found several
	   1995-1996 era Pentium systems with cheap PCI controllers that don't decode all
	   the bus bits. In today's world the motherboard manufacturers are better about
	   that kind of thing and I don't expect to see device aliasing when enumerating
	   all 256 busses reported by the ACPI BIOS... but if I do catch such a system I
	   will add code here to deal with it */
}

/* PCI Express uses a memory-mapped region rather than I/O, so our access methods are more
   concerned with how 16-bit apps can reach into extended memory or how 32-bit apps can
   reach above 4GB */
int pcie_probe(int preference) {
	unsigned char above4gb=0;
	uint32_t tmp32,tmplen;
	unsigned int i,max;
	uint64_t addr;
	char tmp[32];

	if (pcie_cfg_probed)
		return pcie_cfg;

	/* Require a Pentium MMX or higher, because the 64-bit I/O function uses the MOVQ instruction */
	/* Remove this restriction if there are 486-class embedded boxes with PCI Express */
	if (cpu_basic_level < 5)
		return PCIE_CFG_NONE;

	/* it must have CPUID because... */
	if (!(cpu_flags & CPU_FLAG_CPUID))
		return PCIE_CFG_NONE;

	/* we require MMX for our 64-bit I/O function */
	if (!(cpu_cpuid_features.a.raw[2] & (1UL << 23UL)))
		return PCIE_CFG_NONE;

	/* locating PCI Express on x86 systems requires the use of ACPI BIOS tables */
	if (!acpi_probe() || acpi_rsdp == NULL || acpi_rsdt == NULL)
		return PCIE_CFG_NONE;

	/* OK, locate the MCFG table */
	pcie_cfg_probed = 1;
	pcie_bios_last_bus = 0;
	pcie_cfg = PCIE_CFG_NONE;
	pcie_acpi_mcfg_table = 0;
	pcie_bus_decode_bits = 8; /* modern systems don't have the cheap comparison logic that mid 1990's hardware used to have */
	max = acpi_rsdt_entries();
	for (i=0;pcie_acpi_mcfg_table == 0ULL && i < max;i++) {
		addr = acpi_rsdt_entry(i);
		if (addr == 0ULL) continue;

		tmp32 = acpi_mem_readd(addr); tmplen = 0;
		memcpy(tmp,&tmp32,4); tmp[4] = 0;
		if (acpi_probe_rsdt_check(addr,tmp32,&tmplen)) {
			struct acpi_rsdt_header sdth;

			memset(&sdth,0,sizeof(sdth));
			acpi_memcpy_from_phys(&sdth,addr,sizeof(struct acpi_rsdt_header));
			if (!memcmp(sdth.signature,"MCFG",4)) {
				struct acpi_mcfg_entry entry;
				uint64_t o = addr + 44ULL;
				unsigned int count;
				unsigned int bus;

				pcie_acpi_mcfg_table = addr;
				assert(sizeof(struct acpi_mcfg_entry) == 16);
				count = (unsigned int)(tmplen / sizeof(struct acpi_mcfg_entry));
				while (count != 0) {
					memset(&entry,0,sizeof(entry));
					acpi_memcpy_from_phys(&entry,o,sizeof(struct acpi_mcfg_entry));
					o += sizeof(struct acpi_mcfg_entry);

					/* Some bioses I test against seem to return enough for 3 but fill in only 1? */
					if (entry.base_address != 0ULL || entry.start_pci_bus_number != 0 || entry.end_pci_bus_number != 0) {
						if (entry.start_pci_bus_number > entry.end_pci_bus_number)
							entry.start_pci_bus_number = entry.end_pci_bus_number;
						if (pcie_bios_last_bus < entry.end_pci_bus_number)
							pcie_bios_last_bus = entry.end_pci_bus_number;

						for (bus=entry.start_pci_bus_number;bus <= entry.end_pci_bus_number;bus++) {
							pcie_bus_mmio_base[bus] = entry.base_address +
								(((uint64_t)(bus - entry.start_pci_bus_number)) << 20ULL);

							if (pcie_bus_mmio_base[bus] >= 0xFFF00000ULL)
								above4gb = 1;
						}
					}

					count--;
				}
			}
		}
	}

	if (pcie_acpi_mcfg_table == 0ULL)
		return PCIE_CFG_NONE;

	/* pick a memory access method. use Flat Real Mode because llmem imposes a CPU mode-switch overhead per I/O.
	   however if the BIOS put the MMIO ranges above 4GB we have no choice but to use LLMEM.
	   for 32-bit builds, the ideal method is to take advantage of the DOS extender's preference NOT to use
	   paging and to typecast pointers in the flat 4GB address space */
	if (!above4gb && flatrealmode_allowed())
		pcie_cfg = PCIE_CFG_FLATREAL;
#if TARGET_MSDOS == 32
	else if (!above4gb && !dos_ltp_info.paging)
		pcie_cfg = PCIE_CFG_PTR;
#endif
	else if (llmem_available)
		pcie_cfg = PCIE_CFG_LLMEM;
	else
		return PCIE_CFG_NONE;

	return pcie_cfg;
}

uint32_t (*pcie_read_cfg_array[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint8_t size) = {
	pcie_read_cfg_NOTIMPL,		/* NONE */
	pcie_read_cfg_FLATREAL,		/* PCIE_CFG_FLATREAL */
	pcie_read_cfg_NOTIMPL,		/* PCIE_CFG_LLMEM */
	pcie_read_cfg_PTR,		/* PCIE_CFG_PTR */
};

uint64_t (*pcie_read_cfg_arrayq[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg) = {
	pcie_read_cfg_NOTIMPLq,		/* NONE */
	pcie_read_cfg_NOTIMPLq,		/* PCIE_CFG_FLATREAL */
	pcie_read_cfg_NOTIMPLq,		/* PCIE_CFG_LLMEM */
	pcie_read_cfg_PTRq,		/* PCIE_CFG_PTR */
};

void (*pcie_write_cfg_array[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint32_t data,uint8_t size) = {
	pcie_write_cfg_NOTIMPL,		/* NONE */
	pcie_write_cfg_FLATREAL,	/* PCIE_CFG_FLATREAL */
	pcie_write_cfg_NOTIMPL,		/* PCIE_CFG_LLMEM */
	pcie_write_cfg_PTR,		/* PCIE_CFG_PTR */
};

void (*pcie_write_cfg_arrayq[PCIE_CFG_MAX])(uint8_t bus,uint8_t card,uint8_t func,uint16_t reg,uint64_t data) = {
	pcie_write_cfg_NOTIMPLq,	/* NONE */
	pcie_write_cfg_NOTIMPLq,	/* PCIE_CFG_FLATREAL */
	pcie_write_cfg_NOTIMPLq,	/* PCIE_CFG_LLMEM */
	pcie_write_cfg_PTRq,		/* PCIE_CFG_PTR */
};

uint8_t pcie_probe_device_functions(uint8_t bus,uint8_t dev) {
	uint8_t funcs=8,f;
	uint32_t id[8],bar[8],sub[8];

	/* FIXME: Is this necessary, or are there PCIe devices that are still cheap about decoding the bus/device fields? */

	for (f=0;f < 8;f++) { /* PCI-Express still has 8 functions per device */
		id[f] = pcie_read_cfgl(bus,dev,f,0x00);
		bar[f] = pcie_read_cfgl(bus,dev,f,0x10);
		sub[f] = pcie_read_cfgl(bus,dev,f,0x2C);
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

