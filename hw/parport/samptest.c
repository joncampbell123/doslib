
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/dos/doswin.h>
#include <hw/parport/parport.h>

#include <hw/isapnp/isapnp.h>
#include <hw/parport/parpnp.h>

static struct info_parport *sprt = NULL;

/* NTS: Not suitable for reads that take longer than 1/18.2th of a second.
 * this function is better suited to establishing a baseline speed from small
 * read tests that are very likely to fall within the 18.2Hz tick rate of the
 * system timer. also, bytecount is 16-bit on real-mode builds and cannot
 * count beyond 64KB.
 *
 * WARNING: do not call with bytecount == 0! */
unsigned int tight_read_test(unsigned int bytecount,unsigned int io_port) {
	unsigned int b_tim,e_tim;

	b_tim = read_8254(0);
#if TARGET_MSDOS == 32
	__asm {
		cli
		push		ecx
		push		edx
		mov		ecx,bytecount
		mov		edx,io_port
l1:		in		al,dx		; tight loop: read as FAST as you can!
		loop		l1
		pop		edx
		pop		ecx
		sti
	}
#else
	__asm {
		cli
		push		cx
		push		dx
		mov		cx,bytecount
		mov		dx,io_port
l1:		in		al,dx		; tight loop: read as FAST as you can!
		loop		l1
		pop		dx
		pop		cx
		sti
	}
#endif
	e_tim = read_8254(0);

	/* elapsed time? remember the timer counts downward! and wraps around within 16 bits */
	b_tim = (b_tim - e_tim) & 0xFFFF;

	return b_tim;
}

/* NTS: Not suitable for reads that take longer than 1/18.2th of a second.
 * this function is better suited to establishing a baseline speed from small
 * read tests that are very likely to fall within the 18.2Hz tick rate of the
 * system timer. also, bytecount is 16-bit on real-mode builds and cannot
 * count beyond 64KB.
 *
 * WARNING: do not call with bytecount == 0! */
unsigned int tight_memstore_read_test(unsigned int bytecount,unsigned int io_port) {
	unsigned int b_tim,e_tim;
	unsigned char c=0xFF;

	b_tim = read_8254(0);
#if TARGET_MSDOS == 32
	__asm {
		cli
		push		esi
		push		ecx
		push		edx
		mov		ecx,bytecount
		mov		edx,io_port
l1:		in		al,dx		; tight loop: read as FAST as you can!
		mov		c,al
		inc		esi
		loop		l1
		pop		edx
		pop		ecx
		pop		esi
		sti
	}
#else
	__asm {
		cli
		push		si
		push		cx
		push		dx
		mov		cx,bytecount
		mov		dx,io_port
l1:		in		al,dx		; tight loop: read as FAST as you can!
		mov		c,al
		inc		si
		loop		l1
		pop		dx
		pop		cx
		pop		si
		sti
	}
#endif
	e_tim = read_8254(0);

	/* elapsed time? remember the timer counts downward! and wraps around within 16 bits */
	b_tim = (b_tim - e_tim) & 0xFFFF;

	return b_tim;
}

static unsigned int statr_test_reads[] = {512,1024,2048,4096,6000,8192,12000,16384,23000,32768,46000,65500};

int main() {
	unsigned long time_tc,time_rb; /* time passed, in tc */
	double statr_rate,statr_time;
	int i,avgcount=0;
	double avgsum=0;

	printf("PC parallel printer port test program (print)\n");

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

	if (init_isa_pnp_bios()) {
		if (find_isa_pnp_bios()) {
			printf("ISA PnP BIOS found, probing for PnP ports\n");
			pnp_parport_scan();
		}
	}

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

	printf("Found parallel ports:\n");
	for (i=0;i < info_parports;i++) {
		struct info_parport *prt = &info_parport[i];
		printf(" [%u] port=0x%04x IRQ=%d DMA=%d 10-bit=%u max-xfer-size=%u type='%s'\n",i+1,prt->port,prt->irq,prt->dma,prt->bit10,prt->max_xfer_size,parport_type_str[prt->type]);
	}

	printf("Choice? "); fflush(stdout);
	i = 1; scanf("%d",&i); i--;
	if (i < 0 || i >= info_parports) return 1;
	sprt = &info_parport[i];

	write_8254_system_timer(0); /* make sure the timer is in rate generator mode, full counter */

	avgsum = 0; avgcount = 0;
	printf("Timing test: tight loop (no memory store) reading status port:\n");
	for (i=0;i < (int)(sizeof(statr_test_reads)/sizeof(statr_test_reads[0]));i++) {
		time_rb = statr_test_reads[i];
		if (i != 0) {
			/* compute how long this iteration will take. if we estimate the test
			 * will take longer than 1/25 seconds, stop now. if the test takes
			 * longer than the full countdown interval of the 8254 we cannot time
			 * it properly. */
			double et = (double)time_rb / (avgsum / avgcount);
			if (et > (1.0 / 25)) break;
		}

		time_tc = tight_read_test(time_rb,sprt->port+PARPORT_IO_STATUS);
		statr_time = ((double)time_tc * 1000) / T8254_REF_CLOCK_HZ;
		statr_rate = ((double)1000 * time_rb) / statr_time;
		printf("   ... %lu bytes: %.6f ms (%.1f samp/sec)\n",time_rb,statr_time,statr_rate);
		avgsum += statr_rate; avgcount++;
	}
	avgsum /= avgcount;
	printf("   result >> %.1f samples/sec\n",avgsum);

	avgsum = 0; avgcount = 0;
	printf("Timing test: tight loop (no memory store) reading data port:\n");
	for (i=0;i < (int)(sizeof(statr_test_reads)/sizeof(statr_test_reads[0]));i++) {
		time_rb = statr_test_reads[i];
		if (i != 0) {
			/* compute how long this iteration will take. if we estimate the test
			 * will take longer than 1/25 seconds, stop now. if the test takes
			 * longer than the full countdown interval of the 8254 we cannot time
			 * it properly. */
			double et = (double)time_rb / (avgsum / avgcount);
			if (et > (1.0 / 25)) break;
		}

		time_tc = tight_read_test(time_rb,sprt->port+PARPORT_IO_DATA);
		statr_time = ((double)time_tc * 1000) / T8254_REF_CLOCK_HZ;
		statr_rate = ((double)1000 * time_rb) / statr_time;
		printf("   ... %lu bytes: %.6f ms (%.1f samp/sec)\n",time_rb,statr_time,statr_rate);
		avgsum += statr_rate; avgcount++;
	}
	avgsum /= avgcount;
	printf("   result >> %.1f samples/sec\n",avgsum);


	avgsum = 0; avgcount = 0;
	printf("Timing test: tight loop with memory store reading status port:\n");
	for (i=0;i < (int)(sizeof(statr_test_reads)/sizeof(statr_test_reads[0]));i++) {
		time_rb = statr_test_reads[i];
		if (i != 0) {
			/* compute how long this iteration will take. if we estimate the test
			 * will take longer than 1/25 seconds, stop now. if the test takes
			 * longer than the full countdown interval of the 8254 we cannot time
			 * it properly. */
			double et = (double)time_rb / (avgsum / avgcount);
			if (et > (1.0 / 25)) break;
		}

		time_tc = tight_memstore_read_test(time_rb,sprt->port+PARPORT_IO_STATUS);
		statr_time = ((double)time_tc * 1000) / T8254_REF_CLOCK_HZ;
		statr_rate = ((double)1000 * time_rb) / statr_time;
		printf("   ... %lu bytes: %.6f ms (%.1f samp/sec)\n",time_rb,statr_time,statr_rate);
		avgsum += statr_rate; avgcount++;
	}
	avgsum /= avgcount;
	printf("   result >> %.1f samples/sec\n",avgsum);

	avgsum = 0; avgcount = 0;
	printf("Timing test: tight loop with memory store reading data port:\n");
	for (i=0;i < (int)(sizeof(statr_test_reads)/sizeof(statr_test_reads[0]));i++) {
		time_rb = statr_test_reads[i];
		if (i != 0) {
			/* compute how long this iteration will take. if we estimate the test
			 * will take longer than 1/25 seconds, stop now. if the test takes
			 * longer than the full countdown interval of the 8254 we cannot time
			 * it properly. */
			double et = (double)time_rb / (avgsum / avgcount);
			if (et > (1.0 / 25)) break;
		}

		time_tc = tight_memstore_read_test(time_rb,sprt->port+PARPORT_IO_DATA);
		statr_time = ((double)time_tc * 1000) / T8254_REF_CLOCK_HZ;
		statr_rate = ((double)1000 * time_rb) / statr_time;
		printf("   ... %lu bytes: %.6f ms (%.1f samp/sec)\n",time_rb,statr_time,statr_rate);
		avgsum += statr_rate; avgcount++;
	}
	avgsum /= avgcount;
	printf("   result >> %.1f samples/sec\n",avgsum);

	return 0;
}

