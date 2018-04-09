
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/8237/8237.h>		/* DMA controller */
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>		/* 8259 PIC interrupts */
#include <hw/dos/doswin.h>
#include <hw/floppy/floppy.h>

struct dma_8237_allocation*		floppy_dma = NULL; /* DMA buffer */

char	tmp[1024];

/* chosen geometry */
static signed char              high_density_disk = -1;
static signed char              high_density_drive = -1;
static unsigned char            disk_cyls=0,disk_heads=0,disk_sects=0;
static unsigned short           disk_bps=0;

/* "current position" */
static void (interrupt *my_irq0_old_irq)() = NULL;
static void (interrupt *my_floppy_old_irq)() = NULL;
static struct floppy_controller *my_floppy_irq_floppy = NULL;
static unsigned long floppy_irq_counter = 0;
static int my_floppy_irq_number = -1;

void do_floppy_controller_unhook_irq(struct floppy_controller *fdc);

static void interrupt my_irq0() {
    /* we need to manipulate the BIOS data area to prevent the BIOS from turning off the motor behind our backs */
    /* target values:
     *   40:3F  diskette motor status
     *   40:40  motor shutoff counter */
#if TARGET_MSDOS == 32
    *((unsigned char*)0x43F) = 0x00; /* motors are off */
    *((unsigned char*)0x440) = 0x00; /* keep motor shutoff counter zero */
#else
    *((unsigned char far*)MK_FP(0x40,0x3F)) = 0x00; /* motors are off */
    *((unsigned char far*)MK_FP(0x40,0x40)) = 0x00; /* keep motor shutoff counter zero */
#endif

    my_irq0_old_irq();
}

static void interrupt my_floppy_irq() {
	_cli();
	if (my_floppy_irq_floppy != NULL) {
		my_floppy_irq_floppy->irq_fired++;

		/* ack PIC */
		if (my_floppy_irq_floppy->irq >= 8) p8259_OCW2(8,P8259_OCW2_NON_SPECIFIC_EOI);
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);

		/* If too many IRQs fired, then unhook the IRQ and use polling from now on. */
		if (my_floppy_irq_floppy->irq_fired >= 0xFFFEU) {
			do_floppy_controller_unhook_irq(my_floppy_irq_floppy);
			my_floppy_irq_floppy->irq_fired = ~0; /* make sure the IRQ counter is as large as possible */
		}
	}

	floppy_irq_counter++;
}

int wait_for_enter_or_escape() {
	int c;

	do {
		c = getch();
		if (c == 0) c = getch() << 8;
	} while (!(c == 13 || c == 27));

	return c;
}

void do_floppy_controller_hook_irq(struct floppy_controller *fdc) {
	if (my_floppy_irq_number >= 0 || my_floppy_old_irq != NULL || fdc->irq < 0)
		return;

	/* let the IRQ know what floppy controller */
	my_floppy_irq_floppy = fdc;

	/* enable on floppy controller */
	p8259_mask(fdc->irq);
	floppy_controller_enable_irq(fdc,1);

	/* hook IRQ */
	my_floppy_old_irq = _dos_getvect(irq2int(fdc->irq));
	_dos_setvect(irq2int(fdc->irq),my_floppy_irq);
	my_floppy_irq_number = fdc->irq;

	/* enable at PIC */
	p8259_unmask(fdc->irq);
}

void do_floppy_controller_unhook_irq(struct floppy_controller *fdc) {
	if (my_floppy_irq_number < 0 || my_floppy_old_irq == NULL || fdc->irq < 0)
		return;

	/* disable on floppy controller, then mask at PIC */
	p8259_mask(fdc->irq);
	floppy_controller_enable_irq(fdc,0);

	/* restore the original vector */
	_dos_setvect(irq2int(fdc->irq),my_floppy_old_irq);
	my_floppy_irq_number = -1;
	my_floppy_old_irq = NULL;
}

void do_floppy_controller_enable_irq(struct floppy_controller *fdc,unsigned char en) {
	if (!en || fdc->irq < 0 || fdc->irq != my_floppy_irq_number)
		do_floppy_controller_unhook_irq(fdc);
	if (en && fdc->irq >= 0)
		do_floppy_controller_hook_irq(fdc);
}

int floppy_controller_wait_busy_in_instruction(struct floppy_controller *fdc,unsigned int timeout) {
	do {
		floppy_controller_read_status(fdc);
		if (!floppy_controller_busy_in_instruction(fdc)) return 1;
		t8254_wait(t8254_us2ticks(1000));
	} while (--timeout != 0);

	return 0;
}

int floppy_controller_wait_data_ready_ms(struct floppy_controller *fdc,unsigned int timeout) {
	do {
		floppy_controller_read_status(fdc);
		if (floppy_controller_data_io_ready(fdc)) return 1;
		t8254_wait(t8254_us2ticks(1000));
	} while (--timeout != 0);

	return 0;
}

int floppy_controller_wait_irq(struct floppy_controller *fdc,unsigned int timeout,unsigned int counter) {
	do {
		if (fdc->irq_fired >= counter) break;
		t8254_wait(t8254_us2ticks(1000));
	} while (--timeout != 0);

	return 0;
}

int floppy_controller_write_data(struct floppy_controller *fdc,const unsigned char *data,int len) {
	unsigned int oflags = get_cpu_flags();
	int ret = 0;

	_cli(); /* clear interrupts so we can focus on talking to the FDC */
	while (len > 0) {
		if (!floppy_controller_wait_data_ready(fdc,1000)) {
			if (ret == 0) ret = -1;
			break;
		}
		if (!floppy_controller_can_write_data(fdc)) {
			if (ret == 0) ret = -2;
			break;
		}

		floppy_controller_write_data_byte(fdc,*data++);
		len--; ret++;
	}

	if (oflags&0x200/*IF interrupt enable was on*/) _sti();
	return ret;
}

int floppy_controller_read_data(struct floppy_controller *fdc,unsigned char *data,int len) {
	unsigned int oflags = get_cpu_flags();
	int ret = 0;

	_cli();
	while (len > 0) {
		if (!floppy_controller_wait_data_ready(fdc,1000)) {
			if (ret == 0) ret = -1;
			break;
		}
		if (!floppy_controller_can_read_data(fdc)) {
			if (ret == 0) ret = -2;
			break;
		}

		*data++ = floppy_controller_read_data_byte(fdc);
		len--; ret++;
	}

	if (oflags&0x200/*IF interrupt enable was on*/) _sti();
	return ret;
}

void do_floppy_controller_reset(struct floppy_controller *fdc) {
	floppy_controller_set_reset(fdc,1);
	t8254_wait(t8254_us2ticks(1000000));
	floppy_controller_set_reset(fdc,0);
	floppy_controller_wait_data_ready_ms(fdc,1000);
	floppy_controller_read_status(fdc);
}

void do_spin_up_motor(struct floppy_controller *fdc,unsigned char drv) {
	if (drv > 3) return;

	if (!(fdc->digital_out & (0x10 << drv))) {
		/* if the motor isn't on, then turn it on, and then wait for motor to stabilize */
		floppy_controller_set_motor_state(fdc,drv,1);
		t8254_wait(t8254_us2ticks(500000)); /* 500ms */
	}
}

static void do_read(void) {
    if (high_density_drive < 0) {
        /* TODO: We could read magic values out of CMOS, BIOS, etc. or possibly even ask MS-DOS
         *       whether the drive is 1.2MB/1.44MB high density or 160KB/320KB/360KB double density */
        /* This option only matters if we're asked to read 360KB double density in a 1.2MB high density drive (track numbers).
         * Other formats generally match logical == physical track and our guess is as good as any. */
        high_density_drive = 1; /* pretty likely these days */
    }

    if (high_density_disk < 0) {
        /* TODO: We can try seeking to track 2 and using Read Sector ID to see if Track 1 comes back, to autodetect this. */
        if (disk_cyls > 44)
            high_density_disk = 1;
        else
            high_density_disk = 0;
    }

    if (disk_bps == 0) {
        /* TODO: Use Read Sector ID to determine format */
        /* NTS: Apparently some IBM PC compatible systems, if pushed to do so, can
         *      almost reliably read a PC-98 formatted 1.2MB floppy (1024 bytes/sector). Almost. */
        disk_bps = 512;
    }

    /* disk_bps must be power of 2 */
    /* FIXME: I'm aware the controller has some sort of support for weird formats
     *        with some arbitrary byte count < 255 but I'm not doing to bother with that now */
    if ((disk_bps & (disk_bps - 1)) != 0/*not a power of 2*/ ||
        disk_bps < 128 || disk_bps > 16384) {
        printf("Sectors/track is invalid\n");
        return;
    }

    printf("Disk geometry: CHS %u/%u/%u %u/sector (HD=%d) drive=%s\n",
        disk_cyls,disk_heads,disk_sects,disk_bps,high_density_disk,high_density_drive>0?"HD":"DD");

    if (high_density_disk > 0 && high_density_drive == 0) {
        fprintf(stderr,"That format requires a high density drive\n");
        return;
    }
    if (disk_cyls == 0 || disk_heads == 0 || disk_sects == 0 || disk_bps == 0) {
        fprintf(stderr,"Insufficient information\n");
        return;
    }
}

static void help(void) {
    fprintf(stderr,"test [options]\n");
    fprintf(stderr," -h --help     Show this help\n");
    fprintf(stderr," -1            First controller only\n");
    fprintf(stderr,"These options control the density of the drive, not the media.\n");
    fprintf(stderr,"These options are REQUIRED for 5.25\" drives to read 360KB and 1.2MB properly.\n");
    fprintf(stderr," -hd           Drive is high density (1.2MB, 1.44MB)\n");
    fprintf(stderr," -dd           Drive is double density (360KB)\n");
    fprintf(stderr," -hdisk        Disk is high density\n");
    fprintf(stderr," -ddisk        Disk is double density\n");
    fprintf(stderr," -chs c/h/s    Geometry\n");
    fprintf(stderr," -bs n         Bytes per sector\n");
}

static int parse_argv(int argc,char **argv) {
    char *a;
    int i;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"h") || !strcmp(a,"help")) {
                help();
                return 1;
            }
            else if (!strcmp(a,"1")) {
                floppy_controllers_enable_2nd = 0;
            }
            else if (!strcmp(a,"bs")) {
                a = argv[i++];
                if (a == NULL) return 1;
                disk_bps = strtoul(a,&a,10);
            }
            else if (!strcmp(a,"chs")) {
                a = argv[i++];
                if (a == NULL) return 1;

                disk_cyls = strtoul(a,&a,10);
                if (*a == '/') a++;
                disk_heads = strtoul(a,&a,10);
                if (*a == '/') a++;
                disk_sects = strtoul(a,&a,10);
            }
            else if (!strcmp(a,"hd")) {
                high_density_drive = 1;
            }
            else if (!strcmp(a,"dd")) {
                high_density_drive = 0;
            }
            else if (!strcmp(a,"hdisk")) {
                high_density_disk = 1;
            }
            else if (!strcmp(a,"ddisk")) {
                high_density_disk = 0;
            }
            else {
                fprintf(stderr,"Unknown switch '%s'\n",a);
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unknown switch '%s'\n",a);
            return 1;
        }
    }

    return 0;
}

int main(int argc,char **argv) {
	struct floppy_controller *reffdc;
	struct floppy_controller *newfdc;
	struct floppy_controller *floppy;
    unsigned char drive = 0;
	int i;

    if (parse_argv(argc,argv))
        return 1;

	if (!probe_8237()) {
		printf("WARNING: Cannot init 8237 DMA\n");
        return 1;
    }
	if (!probe_8254()) {
		printf("8254 chip not detected\n");
		return 1;
	}
	if (!probe_8259()) {
		printf("8259 chip not detected\n");
		return 1;
	}
	if (!init_floppy_controller_lib()) {
		printf("Failed to init floppy controller\n");
		return 1;
	}

	printf("Probing standard FDC ports...\n");
	for (i=0;(reffdc = (struct floppy_controller*)floppy_get_standard_isa_port(i)) != NULL;i++) {
		printf("   %3X IRQ %d DMA %d: ",reffdc->base_io,reffdc->irq,reffdc->dma); fflush(stdout);

		if ((newfdc = floppy_controller_probe(reffdc)) != NULL) {
			printf("FOUND. PS/2=%u AT=%u dma=%u DOR/RW=%u DOR=%02xh DIR=%02xh mst=%02xh\n",
				newfdc->ps2_mode,
				newfdc->at_mode,
				newfdc->use_dma,
				newfdc->digital_out_rw,
				newfdc->digital_out,
				newfdc->digital_in,
				newfdc->main_status);
		}
		else {
			printf("\x0D                             \x0D"); fflush(stdout);
		}
	}

    floppy = floppy_get_controller(0);

	if (floppy == NULL) {
        fprintf(stderr,"Failed to find floppy controller\n");
		free_floppy_controller_lib();
		return 0;
	}

    /* allocate DMA */
	if (floppy->dma >= 0 && floppy_dma == NULL) {
#if TARGET_MSDOS == 32
		uint32_t choice = 65536;
#else
		uint32_t choice = 32768;
#endif

		do {
			floppy_dma = dma_8237_alloc_buffer(choice);
			if (floppy_dma == NULL) choice -= 4096UL;
		} while (floppy_dma == NULL && choice > 4096UL);

		if (floppy_dma == NULL) {
            fprintf(stderr,"Unable to alloc DMA\n");
            return 0;
        }
	}

    my_irq0_old_irq = _dos_getvect(irq2int(0));
    _dos_setvect(irq2int(0),my_irq0);

	/* if the floppy struct says to use interrupts, then do it */
	do_floppy_controller_enable_irq(floppy,floppy->use_irq);
	floppy_controller_enable_dma(floppy,floppy->use_dma);

    /* do the read */
    do_read();

    /* switch off motor */
    floppy_controller_set_motor_state(floppy,drive,0);

    /* free DMA */
	if (floppy_dma != NULL) {
		dma_8237_free_buffer(floppy_dma);
		floppy_dma = NULL;
	}

    /* more */
    do_floppy_controller_enable_irq(floppy,0);
	floppy_controller_enable_irqdma_gate_otr(floppy,1); /* because BIOSes probably won't */
	p8259_unmask(floppy->irq);

    /* restore IRQ */
    _dos_setvect(irq2int(0),my_irq0_old_irq);

	free_floppy_controller_lib();
	return 0;
}

