
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/flatreal/flatreal.h>
#include <hw/dos/doswin.h>

#include <hw/cpu/apiclib.h>

int main(int argc,char **argv) {
	if (!probe_apic()) {
		printf("APIC not detected. Reason: %s\n",apic_error_str);
		return 1;
	}

	printf("APIC base address:        0x%08lx\n",(unsigned long)apic_base);
	printf("APIC global enable:       %u\n",(unsigned int)(apic_flags&APIC_FLAG_GLOBAL_ENABLE?1:0));
	printf("Read from bootstrap CPU:  %u\n",(unsigned int)(apic_flags&APIC_FLAG_PROBE_ON_BOOT_CPU?1:1));

#if TARGET_MSDOS == 32
	dos_ltp_probe();
	/* TODO: If LTP probe indicates we shouldn't assume physical<->linear addresses (i.e paging) then bail out */
#else
	probe_dos();
	detect_windows();
	if (!flatrealmode_setup(FLATREALMODE_4GB)) {
		printf("Cannot enter flat real mode\n");
		return 1;
	}
#endif

	{
		unsigned int i;

		/* NTS: For safe access on Intel processors always read on 16-byte aligned boundary, 32-bit at all times.
		 * Intel warns the undefined bytes 4-15 between the regs are undefined and may cause undefined behavior. */
		printf("APIC dump:\n");
		for (i=0x0;i < 0x400;i += 16) {
			if ((i&0x7F) == 0)
				printf("0x%03x:",i);

			printf("%08lx ",(unsigned long)apic_readd(i));
			if ((i&0x7F) == 0x70)
				printf("\n");
		}
	}

	return 0;
}

