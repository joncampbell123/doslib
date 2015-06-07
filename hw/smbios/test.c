/* FIXME:
 *   On some newer Toshiba laptops I own, 16-bit real-mode versions of this code are unable
 *   to read SMBIOS/DMI structures. Why? */

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

int main() {
	struct smbios_struct_entry sen;
	unsigned int i,j;
	char tmpstr[257];
	uint32_t ofs;

	printf("DMI/SMBIOS test program\n");

	cpu_probe();		/* ..for the DOS probe routine */
	probe_dos();		/* ..for the Windows detection code */
	detect_windows();	/* Windows virtualizes the LPT ports, and we don't want probing to occur to avoid any disruption */
#if TARGET_MSDOS == 32
	probe_dpmi();
	dos_ltp_probe();
#endif

	if (!smbios_scan()) {
		printf("SMBIOS/DMI signature not found\n");
		return 1;
	}

	printf("SMBIOS entry point at 0x%08lx len=%u v%u.%u max-struct-size=%u rev=%u\n",(unsigned long)smbios_entry_point,smbios_entry.length,
		smbios_entry.major_version,smbios_entry.minor_version,smbios_entry.maximum_structure_size,
		smbios_entry.entry_point_revision);
	printf("  structure table length=%u address=0x%08lX structures=%u BCD=0x%02x\n  method=%s\n",
		smbios_entry.structure_table_length,
		smbios_entry.structure_table_address,
		smbios_entry.number_of_smbios_structures,
		smbios_entry.smbios_bcd_revision,
		smbios_access_to_str(smbios_access));

	for (i=0,ofs=0UL;i < smbios_entry.number_of_smbios_structures;i++) {
		if (!smbios_next_entry(ofs,&sen)) {
			printf("Premature end\n");
			break;
		}

		printf("@%lu Type 0x%02x Len 0x%02x Handle=0x%04x %u strings (%s)\n",
			(unsigned long)ofs,
			sen.type,sen.length,sen.handle,sen.strings,
			smbios_type_to_str(sen.type));

		if (sen.type == 0) { /* BIOS information */
			struct smbios_bios_info bi;

			for (j=0;j < (unsigned int)sizeof(bi);j++)
				((uint8_t*)(&bi))[j] = smbios_peek(ofs+j);

			if (bi.vendor_str_idx != 0 && bi.vendor_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.vendor_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("  Vendor: '%s' (%u)\n",tmpstr,bi.vendor_str_idx);
			if (bi.bios_version_str_idx != 0 && bi.bios_version_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.bios_version_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("  BIOS version: '%s' (%u)\n",tmpstr,bi.bios_version_str_idx);
			printf("  BIOS starting addr: 0x%05lx\n",(unsigned long)bi.bios_starting_address_segment << 4UL);
			if (bi.bios_release_data_str_idx != 0 && bi.bios_release_data_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.bios_release_data_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("  BIOS release data: '%s' (%u)\n",tmpstr,bi.bios_release_data_str_idx);
			printf("  BIOS ROM size: %uKB\n",64 * ((unsigned int)bi.bios_rom_size + 1));
			printf("  BIOS characteristics: 0x%016llx\n",(unsigned long long)bi.bios_characteristics);
			printf("    ");
			if (bi.bios_characteristics & SMBIOS_BIOS_CF_ISA) printf("ISA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_MCA) printf("MCA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_EISA) printf("EISA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_PCI) printf("PCI ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_PCMCIA) printf("PCMCIA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_PnP) printf("PnP ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_APM) printf("APM ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_IS_UPGRADEABLE) printf("Upgradeable ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_SHADOWING_ALLOWED) printf("Shadowable ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_VL_VESA) printf("VL-VESA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_ESCD_AVAILABLE) printf("ESCD ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_BOOT_FROM_CD) printf("BootFromCD ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_SELECTABLE_BOOT) printf("SelectableBOOT ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_BIOS_ROM_SOCKETED) printf("BIOSROMSocketed ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_BOOT_FROM_PCMCIA) printf("BootFromPCMCIA ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_EDD_SUPPORTED) printf("EDD ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_NEC_9800_FLOPPY) printf("NEC9800Floppy ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_TOSHIBA_1_2MB) printf("Toshiba1.2MB ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_5_25_360KB) printf("5.25/360KB ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_5_25_1_2MB) printf("5.25/1.2MB ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_3_5_720KB) printf("3.5/720KB ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT13_3_5_288MB) printf("3.5/2.88MB ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT5_PRINT_SCREEN) printf("PrintScreen ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT9_8042_KEYBOARD) printf("8042Keyboard ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT14_SERIAL) printf("INT14Serial ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT17_PRINTER) printf("INT17Printer ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_INT10_CGA_MONO) printf("INT10CGAMono ");
 			if (bi.bios_characteristics & SMBIOS_BIOS_CF_NEC_PC98) printf("NEC-PC98 ");
			printf("\n");
		}
		else if (sen.type == 1) {
			struct smbios_system_info bi;

			for (j=0;j < (unsigned int)sizeof(bi);j++)
				((uint8_t*)(&bi))[j] = smbios_peek(ofs+j);

			if (bi.manufacturer_str_idx != 0 && bi.manufacturer_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.manufacturer_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("  Manufacturer: '%s' (%u)\n",tmpstr,bi.manufacturer_str_idx);

			if (bi.product_name_str_idx != 0 && bi.product_name_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.product_name_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("  Product name: '%s' (%u)\n",tmpstr,bi.product_name_str_idx);

			if (bi.version_str_idx != 0 && bi.version_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.version_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("       Version: '%s' (%u)\n",tmpstr,bi.version_str_idx);

			if (bi.serial_number_str_idx != 0 && bi.serial_number_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.serial_number_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("    Serial No.: '%s' (%u)\n",tmpstr,bi.serial_number_str_idx);

			printf("          UUID: {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}\n",
				(unsigned long)bi.uuid.a,
				(unsigned int)bi.uuid.b,
				(unsigned int)bi.uuid.c,
				bi.uuid.d[0],bi.uuid.d[1],
				bi.uuid.d[2],bi.uuid.d[3],bi.uuid.d[4],bi.uuid.d[5],bi.uuid.d[6],bi.uuid.d[7]);
			printf("  Wake up type: %s (%u)\n",smbios_wake_up_type(bi.wake_up_type),bi.wake_up_type);

			if (bi.sku_number_str_idx != 0 && bi.sku_number_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.sku_number_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("       SKU No.: '%s' (%u)\n",tmpstr,bi.sku_number_str_idx);

			if (bi.family_str_idx != 0 && bi.family_str_idx <= sen.strings)
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[bi.family_str_idx-1]);
			else
				tmpstr[0] = 0;
			printf("        Family: '%s' (%u)\n",tmpstr,bi.family_str_idx);
		}
		else {
			for (j=0;j < sen.strings;j++) {
				smbios_get_string(tmpstr,sizeof(tmpstr),ofs+sen.str_ofs[j]);
				printf("  str(%u) = '%s' @%u\n",j,tmpstr,sen.str_ofs[j]);
			}
		}

		ofs += sen.total_length;
	}

	return 0;
}

