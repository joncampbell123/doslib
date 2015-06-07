
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
#include <hw/dos/doswin.h>
#include <hw/parport/parport.h>

#if defined(ISAPNP)
#include <hw/isapnp/isapnp.h>
#include <hw/parport/parpnp.h>
#endif

int main() {
	int i;

	printf("PC parallel printer port test program\n");
#ifdef ISAPNP
	printf("ISA Plug & Play version\n");
#endif

	cpu_probe();		/* ..for the DOS probe routine */
	probe_dos();		/* ..for the Windows detection code */
	detect_windows();	/* Windows virtualizes the LPT ports, and we don't want probing to occur to avoid any disruption */

	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}

	if (!probe_8259()) {
		printf("8259 not found (I need this for portions of the test involving serial interrupts)\n");
		return 1;
	}

	if (!init_parport()) {
		printf("Cannot init parport library\n");
		return 1;
	}

#ifdef ISAPNP
	if (!init_isa_pnp_bios()) {
		printf("Cannot init ISA PnP\n");
		return 1;
	}
	if (find_isa_pnp_bios()) pnp_parport_scan();
	else printf("Warning, ISA PnP BIOS not found\n");
#else
	printf("Probing BIOS-listed locations ");
	for (i=0;i < bios_parports;i++) {
		uint16_t port = get_bios_parport(i);
		printf("%03x ",port);
		if (probe_parport(port)) printf("[OK] ");
		fflush(stdout);
	}
	printf("\n");

	printf("Probing standard port locations ");
	for (i=0;i < STANDARD_PARPORT_PORTS;i++) {
		uint16_t port = standard_parport_ports[i];
		printf("%03x ",port);
		if (probe_parport(port)) printf("[OK] ");
		fflush(stdout);
	}
	printf("\n");
#endif

	printf("Found parallel ports:\n");
	for (i=0;i < info_parports;i++) {
		struct info_parport *prt = &info_parport[i];
		printf(" [%u] port=0x%04x IRQ=%d DMA=%d 10-bit=%u max-xfer-size=%u type='%s'\n",i+1,prt->port,prt->irq,prt->dma,prt->bit10,prt->max_xfer_size,parport_type_str[prt->type]);
	}

	return 0;
}

