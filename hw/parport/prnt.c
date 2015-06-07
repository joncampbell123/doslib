
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

#if defined(ISAPNP)
#include <hw/isapnp/isapnp.h>
#include <hw/parport/parpnp.h>
#endif

static struct info_parport *sprt = NULL;

/* MODE: Standard unidirectional Parallel Port
 * USE:  IBM compatible parallel port printer
 * SPEED: 150 KB/sec
 * COMPATIBLE: All parallel ports */
int std_printer_write_parport(struct info_parport *prt,
#if TARGET_MSDOS == 32
const char *buf
#else
const char far *buf
#endif
,size_t len) {
	const signed long onesec = t8254_us2ticks(1000000UL); /* 1 second's worth of timer ticks */
	const signed long timeout_sec_init = onesec * 10L; /* 10 seconds */
	const unsigned long ms1 = t8254_us2ticks(10); /* I laugh at OSDev wiki for suggesting I wait 10ms! Most sources say at least 0.5uS */
	signed long timeout_sec = timeout_sec_init;
	t8254_time_t timer,ctimer;
	unsigned char ctl,st;
	int ret = 0;

	errno = 0;
	if (len == 0U)
		return 0;

	/* NTS: remember the timer return value counts DOWN, not UP */
	/* NTS: counting code borrowed from 8254 wait function */
#define TMTRK ctimer = read_8254(0);\
	if (ctimer < timer) timeout_sec -= (signed long)(timer - ctimer);\
	else timeout_sec -= (signed long)(((uint16_t)timer + (uint16_t)t8254_counter[0] - (uint16_t)ctimer) & (uint16_t)0xFFFFU);\
	timer = ctimer

	timer = read_8254(0);
	ctl = inp(prt->port+PARPORT_IO_CONTROL) & ~(PARPORT_CTL_STROBE);
	outp(prt->port+PARPORT_IO_CONTROL,ctl);

	do {
		/* wait for not busy */
		do {
			st = inp(prt->port+PARPORT_IO_STATUS);
			if (st & PARPORT_STATUS_nBUSY) break; /* NTS: Various docs will confuse the fuck out of you over this, but what this bit really means is that when set the printer is NOT busy */
			TMTRK;
			if (timeout_sec < (signed long)(0L)) goto timeout;
		} while (1);

		/* write data */
		outp(prt->port+PARPORT_IO_DATA,*buf++);
		ret++;

		/* strobe for at least 1us */
		outp(prt->port+PARPORT_IO_CONTROL,ctl | PARPORT_CTL_STROBE);
		TMTRK;
		t8254_wait(ms1); timer = read_8254(0);
		outp(prt->port+PARPORT_IO_CONTROL,ctl);

		/* now: some sources say it is done when BUSY goes low, and some
		 *      say that the printer might hold BUSY high but signal ACK.
		 *      whatever, we'll break out when either is true.
		 *
		 *      if neither happens, then it counts as a timeout. */
		do {
			st = inp(prt->port+PARPORT_IO_STATUS);
			if (st & PARPORT_STATUS_nBUSY) break; /* NTS: Various docs will confuse the fuck out of you over this, but what this bit really means is that when set the printer is NOT busy */
			else if (!(st & PARPORT_STATUS_nACK)) break; /* Likewise, when this bit is NOT set the printer has acknowledged the byte */
			TMTRK;
			if (timeout_sec < (signed long)(0L)) goto timeout;
		} while (1);

		if (!(st & PARPORT_STATUS_nERROR)) goto error; /* NTS: Again.... if NOT set the printer reports an error. Nice of them to invert certain bits, eh? */
	} while (--len != 0U);

#undef TMTRK
	return ret > 0 ? ret : -1;
timeout:
	errno = EBUSY;
	return ret > 0 ? ret : -1;
error:
	errno = EIO;
	return ret > 0 ? ret : -1;
}

int init_select_printer_parport(struct info_parport *sprt) {
	/* reset the printer */
	outp(sprt->port+PARPORT_IO_CONTROL,PARPORT_CTL_SELECT_PRINTER); /* select printer, init (bit 2 == 0) */
	t8254_wait(t8254_us2ticks(100000UL)); /* 100ms */
	outp(sprt->port+PARPORT_IO_CONTROL,PARPORT_CTL_SELECT_PRINTER | PARPORT_CTL_nINIT); /* select printer, not init */
	t8254_wait(t8254_us2ticks(10000UL)); /* 10ms */
	return (inp(sprt->port+PARPORT_IO_STATUS) & PARPORT_STATUS_SELECT);
}

int main() {
	char buf[1024],c;
	int i,rd,x;

	printf("PC parallel printer port test program (print)\n");
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

	printf("Choice? "); fflush(stdout);
	i = 1; scanf("%d",&i); i--;
	if (i < 0 || i >= info_parports) return 1;
	sprt = &info_parport[i];

	/* this function will return zero if the SELECT line does not raise, usually the sign
	 * a printer is not attached or some non-printer thing is connected */
	if (!init_select_printer_parport(sprt)) {
		fprintf(stderr,"WARNING: I don't see the printer raising the SELECT line in return.\n");
		fprintf(stderr,"         Usually this means the device is a non-printer, or nothing is\n");
		fprintf(stderr,"         attached to the port.\n");
	}

	while (1) {
		printf("1. print test message    2. print user input   3. Reinitialize  4. Formfeed\n");
		c = getch();
		if (c == 27)
			break;
		else if (c == '1') {
			const char *msg = "Testing testing 1 2 3.\r\n" "The quick brown fox jumps over the fence\r\n" "\x0C"; /* page feed */
			printf("Sending...\n");
			rd = strlen(msg);
			x = std_printer_write_parport(sprt,msg,rd);
			if (x < rd) printf("Timeout writing message. Len=%u wrote=%d\n",rd,x);
		}
		else if (c == '2') {
			printf("What do you want to send? (type below %u max)\n",sizeof(buf)-1);
			buf[0] = 0; fgets(buf,sizeof(buf)-1,stdin);
			printf("Sending '%s'\n",buf);
			rd = strlen(buf);
			x = std_printer_write_parport(sprt,buf,rd);
			if (x < rd) printf("Timeout writing message. Len=%u wrote=%d\n",rd,x);
		}
		else if (c == '3') {
			if (!init_select_printer_parport(sprt)) {
				fprintf(stderr,"WARNING: I don't see the printer raising the SELECT line in return.\n");
				fprintf(stderr,"         Usually this means the device is a non-printer, or nothing is\n");
				fprintf(stderr,"         attached to the port.\n");
			}
		}
		else if (c == '4') {
			const char *msg = "\x0C"; /* page feed */
			printf("Sending...\n");
			rd = strlen(msg);
			x = std_printer_write_parport(sprt,msg,rd);
			if (x < rd) printf("Timeout writing message. Len=%u wrote=%d\n",rd,x);
		}
	}

	outp(sprt->port+PARPORT_IO_CONTROL,PARPORT_CTL_nINIT); /* unselect printer, not init */

	return 0;
}

