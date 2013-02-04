/* biosmem.h
 *
 * Various BIOS INT 15h extended memory query functions.
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#pragma pack(push,1)
struct bios_E820 {
	uint64_t	base,length;
	uint32_t	type;
	uint32_t	ext_attributes;
};
#pragma pack(pop)

/* INT 15H AX=E820 types */
enum {
	BIOS_E820_NONE=0,
	BIOS_E820_RAM,
	BIOS_E820_RESERVED,
	BIOS_E820_ACPI_RECLAIMABLE,
	BIOS_E820_ACPI_NVS,
	BIOS_E820_FAULTY
};

int _biosmem_size_E820_3(unsigned long *index,struct bios_E820 *nfo);
int biosmem_size_E820(unsigned long *index,struct bios_E820 *nfo);
int biosmem_size_E801(unsigned int *low,unsigned int *high);
int biosmem_size_88(unsigned int *sz);

