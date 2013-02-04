
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdint.h>

#pragma pack(push,1)
struct smbios_entry_struct {
	char		sm_sig[4];			/* +0 */
	uint8_t		checksum;			/* +4 */
	uint8_t		length;				/* +5 length of this structure */
	uint8_t		major_version,minor_version;	/* +6 */
	uint16_t	maximum_structure_size;		/* +8 */
	uint8_t		entry_point_revision;		/* +10 */
	uint8_t		formatted_area[5];		/* +11 */
	char		dmi_sig[5];			/* +16 _DMI_ */
	uint8_t		intermediate_checksum;		/* +21 */
	uint16_t	structure_table_length;		/* +22 */
	uint32_t	structure_table_address;	/* +24 */
	uint16_t	number_of_smbios_structures;	/* +28 */
	uint8_t		smbios_bcd_revision;		/* +30 */
};

struct smbios_struct_entry {
	uint32_t	offset;
	uint16_t	handle;				/* +2 */
	uint16_t	total_length;
	uint8_t		type,length;			/* +0 */
	uint16_t	str_ofs[256];			/* offset of string relative to entry */
	uint16_t	strings;
};

struct smbios_bios_info {
	uint8_t		type,length;			/* +0 type=0 */
	uint16_t	handle;				/* +2 */
	uint8_t		vendor_str_idx;			/* +4 */
	uint8_t		bios_version_str_idx;		/* +5 */
	uint16_t	bios_starting_address_segment;	/* +6 */
	uint8_t		bios_release_data_str_idx;	/* +8 */
	uint8_t		bios_rom_size;			/* +9 size = 64K*(1+n) */
	uint64_t	bios_characteristics;		/* +10 */
	uint16_t	bios_characteristics_ext;	/* +18 */
	uint8_t		t1,t2,t3,t4;			/* +20 */
};

struct smbios_guid {
	uint32_t	a;				/* +0 */
	uint16_t	b,c;				/* +4 */
	uint8_t		d[8];				/* +8 */
};

struct smbios_system_info {
	uint8_t		type,length;			/* +0 type=1 */
	uint16_t	handle;				/* +2 */
	uint8_t		manufacturer_str_idx;		/* +4 */
	uint8_t		product_name_str_idx;		/* +5 */
	uint8_t		version_str_idx;		/* +6 */
	uint8_t		serial_number_str_idx;		/* +7 */
	struct smbios_guid uuid;			/* +8 */
	uint8_t		wake_up_type;			/* +24 */
	uint8_t		sku_number_str_idx;		/* +25 */
	uint8_t		family_str_idx;			/* +26 */
};
#pragma pack(pop)

enum {
	SMBIOS_ACCESS_DIRECT=0,		/* direct access to the struct */
	SMBIOS_ACCESS_FLAT		/* flat real mode access */
};

extern const char *smbios_access_str[];
#define smbios_access_to_str(x) smbios_access_str[x]

#if TARGET_MSDOS == 32
extern uint8_t				*smbios_table,*smbios_table_fence;
#endif
extern uint32_t				smbios_entry_point;
extern uint8_t				smbios_access;
extern struct smbios_entry_struct	smbios_entry;

const char *smbios_wake_up_type(uint8_t t);
int smbios_next_entry(uint32_t ofs,struct smbios_struct_entry *s);
void smbios_get_string(char *d,size_t dl,uint32_t o);
const char *smbios_type_to_str(uint8_t t);
uint8_t smbios_peek(uint32_t ofs);
int smbios_scan();

#define SMBIOS_BIOS_CF_ISA			(1ULL << 4ULL)
#define SMBIOS_BIOS_CF_MCA			(1ULL << 5ULL)
#define SMBIOS_BIOS_CF_EISA			(1ULL << 6ULL)
#define SMBIOS_BIOS_CF_PCI			(1ULL << 7ULL)
#define SMBIOS_BIOS_CF_PCMCIA			(1ULL << 8ULL)
#define SMBIOS_BIOS_CF_PnP			(1ULL << 9ULL)
#define SMBIOS_BIOS_CF_APM			(1ULL << 10ULL)
#define SMBIOS_BIOS_CF_IS_UPGRADEABLE		(1ULL << 11ULL)
#define SMBIOS_BIOS_CF_SHADOWING_ALLOWED	(1ULL << 12ULL)
#define SMBIOS_BIOS_CF_VL_VESA			(1ULL << 13ULL)
#define SMBIOS_BIOS_CF_ESCD_AVAILABLE		(1ULL << 14ULL)
#define SMBIOS_BIOS_CF_BOOT_FROM_CD		(1ULL << 15ULL)
#define SMBIOS_BIOS_CF_SELECTABLE_BOOT		(1ULL << 16ULL)
#define SMBIOS_BIOS_CF_BIOS_ROM_SOCKETED	(1ULL << 17ULL)
#define SMBIOS_BIOS_CF_BOOT_FROM_PCMCIA		(1ULL << 18ULL)
#define SMBIOS_BIOS_CF_EDD_SUPPORTED		(1ULL << 19ULL)
#define SMBIOS_BIOS_CF_INT13_NEC_9800_FLOPPY	(1ULL << 20ULL)
#define SMBIOS_BIOS_CF_INT13_TOSHIBA_1_2MB	(1ULL << 21ULL)
#define SMBIOS_BIOS_CF_INT13_5_25_360KB		(1ULL << 22ULL)
#define SMBIOS_BIOS_CF_INT13_5_25_1_2MB		(1ULL << 23ULL)
#define SMBIOS_BIOS_CF_INT13_3_5_720KB		(1ULL << 24ULL)
#define SMBIOS_BIOS_CF_INT13_3_5_288MB		(1ULL << 25ULL)
#define SMBIOS_BIOS_CF_INT5_PRINT_SCREEN	(1ULL << 26ULL)
#define SMBIOS_BIOS_CF_INT9_8042_KEYBOARD	(1ULL << 27ULL)
#define SMBIOS_BIOS_CF_INT14_SERIAL		(1ULL << 28ULL)
#define SMBIOS_BIOS_CF_INT17_PRINTER		(1ULL << 29ULL)
#define SMBIOS_BIOS_CF_INT10_CGA_MONO		(1ULL << 30ULL)
#define SMBIOS_BIOS_CF_NEC_PC98			(1ULL << 31ULL)

