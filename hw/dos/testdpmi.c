/* testdpmi.c
 *
 * Test program: DPMI entry/exit functions
 * (C) 2009-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdio.h>
#include <conio.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/dos/dosbox.h>
#include <hw/dos/doswin.h>

#ifdef TARGET_WINDOWS
#error wrong target
#endif

int main(int argc,char **argv) {
	unsigned char want = DPMI_ENTER_AUTO;

	if (argc > 1) {
		want = atoi(argv[1]);
		if (want < 16 || want > 32) return 1;
	}

	probe_dos();
	printf("DOS version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	printf("    Method: '%s'\n",dos_version_method);
	if (detect_windows()) {
		printf("I am running under Windows.\n");
		printf("    Mode: %s\n",windows_mode_str(windows_mode));
		printf("    Ver:  %x.%02u\n",windows_version>>8,windows_version&0xFF);
		printf("    Method: '%s'\n",windows_version_method);
	}
	else {
		printf("Not running under Windows or OS/2\n");
	}

	probe_dpmi();
	if (!dpmi_present) {
		printf("This test requires DPMI\n");
		return 1;
	}

	printf("DPMI present:\n");
#if dpmi_no_0301h != 0
	if (dpmi_no_0301h > 0) printf(" - DPMI function 0301H: Call real-mode far routine NOT AVAILABLE\n");
#endif
	printf(" - Flags: 0x%04x\n",dpmi_flags);
	printf(" - Entry: %04x:%04x (real mode)\n",(unsigned int)(dpmi_entry_point>>16UL),(unsigned int)(dpmi_entry_point & 0xFFFFUL));
	printf(" - Processor type: %02x\n",dpmi_processor_type);
	printf(" - Version: %u.%u\n",dpmi_version>>8,dpmi_version&0xFF);
	printf(" - Private data length: %u paras\n",dpmi_private_data_length_paragraphs);

	/* enter DPMI. the routine will briefly run in protected mode before finding it's way
	 * back to real mode where it can return back to this function */
	if (!dpmi_enter(want)) {
		printf("Unable to enter DPMI server\n");
		return 1;
	}
	printf("Allocated DPMI private segment: 0x%04x\n",dpmi_private_data_segment);
	printf("DPMI entered as %u-bit.\n",dpmi_entered);
	printf(" - PM CS:%04x DS:%04x ES:%04x SS:%04x\n",dpmi_pm_cs,dpmi_pm_ds,dpmi_pm_es,dpmi_pm_ss);
	printf(" - Real to protected entry: %04x:%04x [rmode]\n",
		(unsigned int)(dpmi_pm_entry>>16UL),(unsigned int)(dpmi_pm_entry&0xFFFFUL));
	if (dpmi_entered == 32)
		printf(" - Protected to real entry: %04x:%08lx [pmode]\n",
			(unsigned int)((dpmi_rm_entry>>32ULL)&0xFFFFUL),(unsigned long)(dpmi_rm_entry&0xFFFFFFFFUL));
	else
		printf(" - Protected to real entry: %04x:%04x [pmode]\n",
			(unsigned int)(dpmi_rm_entry>>16UL),(unsigned int)(dpmi_rm_entry&0xFFFFUL));

	return 0;
}

