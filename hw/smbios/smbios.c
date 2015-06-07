
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/smbios/smbios.h>
#include <hw/flatreal/flatreal.h>
#include <hw/dos/doswin.h>

uint8_t				smbios_access=0;
#if TARGET_MSDOS == 32
uint8_t				*smbios_table=NULL,*smbios_table_fence=NULL;
#endif
uint32_t			smbios_entry_point=0;
struct smbios_entry_struct	smbios_entry;

const char *smbios_access_str[] = {
	"direct",
	"flat"
};

const char *smbios_type_to_str(uint8_t t) {
	switch (t) {
		case 0x00: return "BIOS";
		case 0x01: return "System";
		case 0x02: return "Base board";
		case 0x03: return "System enclosure";
		case 0x04: return "Processor";
		case 0x05: return "Memory controller";
		case 0x06: return "Memory module";
		case 0x07: return "Cache";
		case 0x08: return "Port connector";
		case 0x09: return "System slots";
		case 0x0A: return "On-board devices";
		case 0x0B: return "OEM strings";
		case 0x0C: return "System configuration";
		case 0x0D: return "BIOS language";
		case 0x0E: return "Group association";
		case 0x0F: return "System event log";
		case 0x10: return "Physical memory array";
		case 0x11: return "Memory device";
		case 0x12: return "32-bit memory error";
		case 0x13: return "Memory array mapped address";
		case 0x14: return "Memory device mapped address";
		case 0x15: return "Built-in pointing device";
		case 0x16: return "Portable battery";
		case 0x17: return "System reset";
		case 0x18: return "Hardware security";
		case 0x19: return "System power controls";
		case 0x1A: return "Voltage probe";
		case 0x1B: return "Cooling device";
		case 0x1C: return "Temperature probe";
		case 0x1D: return "Electrical current probe";
		case 0x1E: return "Out-of-band remote access";
		case 0x1F: return "Boot integrity services";
		case 0x20: return "System boot";
		case 0x21: return "64-bit memory error";
		case 0x22: return "Management device";
		case 0x23: return "Management device component";
		case 0x24: return "Management device threshold data";
		case 0x25: return "Memory channel";
		case 0x26: return "IPMI device";
		case 0x27: return "System power supply";
		case 0x28: return "Additional information";
		case 0x29: return "Onboard devices extended information";
		case 0x7E: return "Inactive";
		case 0x7F: return "End of table";
	};

	return "";
}

uint8_t smbios_peek(uint32_t ofs) {
#if TARGET_MSDOS == 32
	if (smbios_table == NULL || ofs >= ((size_t)(smbios_table_fence - smbios_table)))
		return 0;

	return smbios_table[ofs];
#else
	if (ofs >= smbios_entry.structure_table_length)
		return 0;

	ofs += smbios_entry.structure_table_address;
	if (smbios_access == SMBIOS_ACCESS_FLAT) {
		if (!flatrealmode_ok() && !flatrealmode_setup(FLATREALMODE_4GB)) return 0;
		return flatrealmode_readb(ofs);
	}
	else if (ofs <= 0xFFFFFUL) {
		return *((unsigned char far*)MK_FP(ofs>>4,ofs&0xF));
	}
#endif

	return 0;
}

void smbios_get_string(char *d,size_t dl,uint32_t o) {
	unsigned int of=0;
	unsigned char c;

	while ((of+1) < (unsigned int)dl) {
		c = smbios_peek(o++);
		if (c == 0) break;
		d[of++] = c;
	}
	d[of] = 0;
}

int smbios_next_entry(uint32_t ofs,struct smbios_struct_entry *s) {
	unsigned int o;

	if ((ofs+4) > smbios_entry.structure_table_length)
		return 0;

	s->offset = ofs;
	s->strings = 0;
	s->type = smbios_peek(ofs+0);
	s->length = smbios_peek(ofs+1);
	s->handle = smbios_peek(ofs+2) | ((uint16_t)smbios_peek(ofs+3) << 8U);
	if ((ofs+s->length) > smbios_entry.structure_table_length) return 0;

	/* then ASCII strings follow */
	o = s->length;
	do {
		if (smbios_peek(ofs+o) == 0 && smbios_peek(ofs+o+1) == 0) {
			o += 2;
			break;
		}
		else if (smbios_peek(ofs+o) == 0) {
			o++;
		}

		if (s->strings < 256) s->str_ofs[s->strings++] = o;
		while (smbios_peek(ofs+o) != 0) o++;
	} while (1);
	s->total_length = o;
	return 1;
}

int smbios_scan() {
	unsigned int o,i;
#if TARGET_MSDOS == 32
	unsigned char *base = (unsigned char*)0xF0000UL;
#else
	unsigned char far *base = (unsigned char far*)MK_FP(0xF000U,0x0000U);
#endif

	assert(sizeof(struct smbios_entry_struct) == 0x1F);

	/* the signature lies on a paragraph boundary between 0xF0000-0xFFFFF and is 0x1F long */
	smbios_entry_point=0;
	for (i=0;i < 0xFFF0;i += 0x10) {
		if (base[i+0] == '_' && base[i+1] == 'S' && base[i+2] == 'M' && base[i+3] == '_') { /* _SM_ */
			unsigned char len = base[i+5],chk;
			if (len >= 0x1E && ((long)i + (long)len) < 0xFFFFL) {
				for (o=0,chk=0;o < (unsigned int)len;o++) {
					((unsigned char*)(&smbios_entry))[o] = base[i+o];
					chk += base[i+o];
				}
				if (chk == 0 && smbios_entry.structure_table_length > 2) {
					/* choose access method */
#if TARGET_MSDOS == 32
					smbios_access = SMBIOS_ACCESS_DIRECT;
					if (!dos_ltp_info.paging)
						smbios_table = (uint8_t*)smbios_entry.structure_table_address;
					else
						smbios_table = NULL;/* TODO */
					if (smbios_table == NULL) continue;
					smbios_table_fence = smbios_table + smbios_entry.structure_table_length;
#else
					/* real mode: if the address is below the 1MB boundary we can access it directly */
					if ((smbios_entry.structure_table_address+smbios_entry.structure_table_length) <= 0xFFFFFUL)
						smbios_access = SMBIOS_ACCESS_DIRECT;
					else if (flatrealmode_allowed())
						smbios_access = SMBIOS_ACCESS_FLAT;
					else
						continue;
#endif

					smbios_entry_point=(uint32_t)i + (uint32_t)0xF0000UL;
					return 1;
				}
			}
		}
	}

	return 0;
}

const char *smbios_wake_up_type(uint8_t t) {
	switch (t) {
		case 0x01: return "Other";
		case 0x02: return "Unknown";
		case 0x03: return "APM timer";
		case 0x04: return "Modem ring";
		case 0x05: return "LAN remote";
		case 0x06: return "Power switch";
		case 0x07: return "PCI PME#";
		case 0x08: return "AC Power restored";
	};

	return "";
}

