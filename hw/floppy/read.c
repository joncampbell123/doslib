
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
static unsigned char            current_phys_head=0;
static unsigned char            disk_cyls=0,disk_heads=0,disk_sects=0;
static unsigned int             disk_drate=0;
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

void do_check_interrupt_status(struct floppy_controller *fdc) {
	char cmd[10],resp[10];
	int rd,wd,rdo,wdo;

	floppy_controller_read_status(fdc);
	if (!floppy_controller_can_write_data(fdc) || floppy_controller_busy_in_instruction(fdc))
		do_floppy_controller_reset(fdc);

	/* Check Interrupt Status (x8h)
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |  0   0   0   0   1   0   0   0
	 *
	 */

	wdo = 1;
	cmd[0] = 0x08;	/* Check interrupt status */
	wd = floppy_controller_write_data(fdc,cmd,wdo);
	if (wd < 1) {
        fprintf(stderr,"Failed to write FDC data\n");
		do_floppy_controller_reset(fdc);
		return;
	}

	/* wait for data ready. does not fire an IRQ (because you use this to clear IRQ status!) */
	floppy_controller_wait_data_ready_ms(fdc,1000);

	/* NTS: It's not specified whether this returns 2 bytes if success and 1 if no IRQ pending.. or...? */
	rdo = 2;
	resp[1] = 0;
	rd = floppy_controller_read_data(fdc,resp,rdo);
	if (rd < 1) {
        fprintf(stderr,"Failed to read FDC data\n");
		do_floppy_controller_reset(fdc);
		return;
	}

	/* Check Interrupt Status (x8h) response
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |              ST0
	 *      1 |        Current Cylinder
	 */

	/* the command SHOULD terminate */
	floppy_controller_wait_data_ready(fdc,20);
	if (!floppy_controller_wait_busy_in_instruction(fdc,1000))
		do_floppy_controller_reset(fdc);

	/* return value is ST0 and the current cylinder */
	fdc->st[0] = resp[0];
	fdc->cylinder = resp[1];
}

void do_calibrate_drive(struct floppy_controller *fdc) {
	char cmd[10];
	int wd,wdo;

	do_spin_up_motor(fdc,fdc->digital_out&3);

	floppy_controller_read_status(fdc);
	if (!floppy_controller_can_write_data(fdc) || floppy_controller_busy_in_instruction(fdc))
		do_floppy_controller_reset(fdc);

	floppy_controller_reset_irq_counter(fdc);

	/* Calibrate Drive (x7h)
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |  0   0   0   0   0   1   1   1
	 *      1 |  x   x   x   x   x   0 DR1 DR0
	 *
	 *  DR1,DR0 = Drive select */

	wdo = 2;
	cmd[0] = 0x07;	/* Calibrate */
	cmd[1] = (fdc->digital_out&3)/* [1:0] = DR1,DR0 */;
	wd = floppy_controller_write_data(fdc,cmd,wdo);
	if (wd < 2) {
		fprintf(stderr,"Failed to write data to FDC, %u/%u",wd,wdo);
		do_floppy_controller_reset(fdc);
		return;
	}

	/* fires an IRQ. doesn't return state */
	if (fdc->use_irq) floppy_controller_wait_irq(fdc,1000,1);
	floppy_controller_wait_data_ready_ms(fdc,1000);

	/* Calibrate Drive (x7h) response
	 *
	 * (none)
	 */

	/* the command SHOULD terminate */
	floppy_controller_wait_data_ready(fdc,20);
	if (!floppy_controller_wait_busy_in_instruction(fdc,1000))
		do_floppy_controller_reset(fdc);

	/* use Check Interrupt Status */
	do_check_interrupt_status(fdc);
}

void do_seek_drive(struct floppy_controller *fdc,uint8_t track) {
	char cmd[10];
	int wd,wdo;

	do_spin_up_motor(fdc,fdc->digital_out&3);

	floppy_controller_read_status(fdc);
	if (!floppy_controller_can_write_data(fdc) || floppy_controller_busy_in_instruction(fdc)) {
        /* Fun fact, on real hardware, resetting the floppy controller also causes the
         * controller to forget which track it's on (reset to zero).
         *
         * So, issuing a command to seek to track 10, resetting the controller, then issuing another seek to track 10
         * will leave the head on track 20. */
		do_floppy_controller_reset(fdc);
        do_calibrate_drive(fdc);
    }

	floppy_controller_reset_irq_counter(fdc);

	/* Seek (xFh)
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |  0   0   0   0   1   1   1   1
	 *      1 |  x   x   x   x   x  HD DR1 DR0
	 *      2 |            Cylinder
	 *
	 *         HD = Head select (on PC platform, doesn't matter)
	 *    DR1,DR0 = Drive select
	 *   Cylinder = Track to move to */

	wdo = 3;
	cmd[0] = 0x0F;	/* Seek */
	cmd[1] = (fdc->digital_out&3)+(current_phys_head?0x04:0x00)/* [1:0] = DR1,DR0 [2:2] HD (doesn't matter) */;
	cmd[2] = track;
	wd = floppy_controller_write_data(fdc,cmd,wdo);
	if (wd < 3) {
		fprintf(stderr,"Failed to write data to FDC, %u/%u",wd,wdo);
		do_floppy_controller_reset(fdc);
		return;
	}

	/* fires an IRQ. doesn't return state */
	if (fdc->use_irq) floppy_controller_wait_irq(fdc,1000,1);
	floppy_controller_wait_data_ready_ms(fdc,1000);

	/* Seek (xFh) response
	 *
	 * (none)
	 */

	/* the command SHOULD terminate */
	floppy_controller_wait_data_ready(fdc,20);
	if (!floppy_controller_wait_busy_in_instruction(fdc,1000))
		do_floppy_controller_reset(fdc);

	/* use Check Interrupt Status */
	do_check_interrupt_status(fdc);
}

int do_read_sector_id(unsigned char resp[7],struct floppy_controller *fdc,unsigned char head) {
	int rd,wd,rdo,wdo;
	char cmd[10];

	floppy_controller_read_status(fdc);
	if (!floppy_controller_can_write_data(fdc) || floppy_controller_busy_in_instruction(fdc))
		do_floppy_controller_reset(fdc);

	/* Read ID (xAh)
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |  0  MF   0   0   1   0   1   0
	 *      1 |  x   x   x   x   x  HD DR1 DR0
	 *
	 *         MF = MFM/FM
	 *         HD = Head select (on PC platform, doesn't matter)
	 *    DR1,DR0 = Drive select */

	wdo = 2;
	cmd[0] = 0x0A + 0x40; /* Read sector ID [6:6] MFM */
	cmd[1] = (fdc->digital_out&3)+(head<<2)/* [1:0] = DR1,DR0 [2:2] = HD */;
	wd = floppy_controller_write_data(fdc,cmd,wdo);
	if (wd < 2) {
		do_floppy_controller_reset(fdc);
		return 0;
	}

	/* wait for data ready. does not fire an IRQ */
	floppy_controller_wait_data_ready_ms(fdc,1000);

	rdo = 7;
	rd = floppy_controller_read_data(fdc,resp,rdo);
	if (rd < 1) {
		do_floppy_controller_reset(fdc);
		return 0;
	}

	/* Read ID (xAh) response
	 *
	 *   Byte |  7   6   5   4   3   2   1   0
	 *   -----+---------------------------------
	 *      0 |              ST0
	 *      1 |              ST1
	 *      2 |              ST2
	 *      3 |           Cylinder
	 *      4 |             Head
	 *      5 |        Sector Number
	 *      6 |         Sector Size
         */

	/* the command SHOULD terminate */
	floppy_controller_wait_data_ready(fdc,20);
	if (!floppy_controller_wait_busy_in_instruction(fdc,1000))
		do_floppy_controller_reset(fdc);

	/* accept ST0..ST2 from response and update */
	if (rd >= 3) {
		fdc->st[0] = resp[0];
		fdc->st[1] = resp[1];
		fdc->st[2] = resp[2];
	}
	if (rd >= 4) {
		fdc->cylinder = resp[3];
	}

	return rd;
}

void apply_drate(struct floppy_controller *fdc,unsigned char drive,unsigned int rate) {
    if (rate > 600)
        floppy_controller_set_data_transfer_rate(fdc,3); /* 1000 */
    else if (rate > 400)
        floppy_controller_set_data_transfer_rate(fdc,0); /* 500 */
    else if (rate > 280)
        floppy_controller_set_data_transfer_rate(fdc,1); /* 300 */
    else
        floppy_controller_set_data_transfer_rate(fdc,2); /* 250 */

    floppy_controller_set_motor_state(fdc,drive,0);
    do_spin_up_motor(fdc,drive);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
}

static unsigned short drate_tests[4] = { 250, 300, 500, 1000 };

static void do_read(struct floppy_controller *fdc,unsigned char drive) {
	unsigned int returned_length = 0;
    unsigned int track_2x = 1;
    unsigned int data_length;
    int r_cyl,r_head,r_sec;
	char cmd[10],resp[10];
    unsigned char wd,wdo;
    unsigned char rd,rdo;
    unsigned char r_ssz;
    int img_fd = -1;

    /* disk_bps must be power of 2 */
    /* FIXME: I'm aware the controller has some sort of support for weird formats
     *        with some arbitrary byte count < 255 but I'm not doing to bother with that now */
    if (disk_bps != 0) {
        if ((disk_bps & (disk_bps - 1)) != 0/*not a power of 2*/ ||
             disk_bps < 128 || disk_bps > 16384) {
            printf("Sectors/track is invalid\n");
            return;
        }
    }

    do_spin_up_motor(fdc,drive);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);

    if (disk_drate > 0)
        apply_drate(fdc,drive,disk_drate);

    if (disk_drate == 0) {
        unsigned char m[4];
        unsigned char mmax;

        /* try to autodetect the date rate.
         * on real hardware the sector IDs will only work at one rate.
         * in emulators, sector IDs will work at any data rate. */

        mmax=0;
        memset(m,0,sizeof(m));
        for (r_sec=0;r_sec < 4;r_sec++) {
            apply_drate(fdc,drive,drate_tests[r_sec]);
            do_seek_drive(fdc,0); /* use track 0 */

            for (r_cyl=0;r_cyl < 2;r_cyl++) {
                if (do_read_sector_id(resp,fdc,0) >= 7 && (fdc->st[0] & 0xC0) == 0/*no error*/) {
                    /*      3 |           Cylinder
                     *      4 |             Head
                     *      5 |        Sector Number
                     *      6 |         Sector Size */
                    if (resp[4] == 0 && resp[5] < 36 && resp[3] == 0/*track 0*/) {
                        m[r_sec]++;
                        if (mmax < m[r_sec]) mmax = m[r_sec];
                    }
                }
                else {
                    r_cyl += 4 - 1;
                }
            }
        }

        /* which one? */
        if (mmax >= 2) {
            r_sec=0;
            while (r_sec < 4 && m[r_sec] < mmax) r_sec++;

            if (r_sec < 4) {
                /* if other data rates work just as well, then our test results don't apply */
                disk_drate = drate_tests[r_sec++];

                while (r_sec < 4) {
                    if (m[r_sec] >= mmax) {
                        disk_drate=0;
                        break;
                    }
                    r_sec++;
                }
            }
        }

        if (disk_drate != 0) {
            fprintf(stderr,"Disk drate autodetected %uk\n",disk_drate);

            apply_drate(fdc,drive,disk_drate);
        }
        else {
            fprintf(stderr,"Disk drate autodetection failed [%u,%u,%u,%u] cannot determine\n",m[0],m[1],m[2],m[3]);

            apply_drate(fdc,drive,500);
        }
    }

    if (disk_sects == 0 || disk_bps == 0) {
        unsigned char presp[4];
        unsigned char ssz[8];
        unsigned char ss[36];
        unsigned int max;

        memset(presp,0,sizeof(presp));
        memset(ssz,0,sizeof(ssz));
        memset(ss,0,sizeof(ss));

        do_spin_up_motor(fdc,drive);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);

        for (r_cyl=0;r_cyl < 255;r_cyl++) {
            if (do_read_sector_id(resp,fdc,0) >= 7 && (fdc->st[0] & 0xC0) == 0/*no error*/) {
                /*      3 |           Cylinder
                 *      4 |             Head
                 *      5 |        Sector Number
                 *      6 |         Sector Size */
                if (resp[4] == 0 && resp[5] < 36 && resp[3] == 0/*track 0*/) {
                    if (presp[0] != resp[3] || presp[1] != resp[4] |
                        presp[2] != resp[5] || presp[3] != resp[6]) {
                        presp[0] = resp[3];
                        presp[1] = resp[4];
                        presp[2] = resp[5];
                        presp[3] = resp[6];

                        if ((unsigned char)resp[6] < 8 && ssz[(unsigned char)resp[6]] < 255)
                            ssz[(unsigned char)resp[6]]++;
                        if ((unsigned char)resp[5] < 36 && ss[(unsigned char)resp[5]] < 255)
                            ss[(unsigned char)resp[5]]++;
                    }
                    else {
                        r_cyl--;
                    }
                }
            }
            else {
                r_cyl += 4 - 1;
            }
        }

        if (disk_bps == 0) {
            max=0;
            for (r_cyl=0;r_cyl < 8;r_cyl++) {
                if (max < ssz[r_cyl]) {
                    max = ssz[r_cyl];
                    if (max >= 4) disk_bps = 128 << r_cyl;
                }
            }
        }

        if (disk_sects == 0) {
            for (r_cyl=0;r_cyl < 36;r_cyl++) {
                if (ss[r_cyl] != 0) disk_sects = r_cyl;
            }
        }
    }

    r_ssz = 0; /* 128 */
    if (disk_bps != 0) {
        while ((disk_bps >> (7+r_ssz)) != 1)
            r_ssz++;
    }

    if (disk_drate == 0) {
        if (disk_sects > 21)
            disk_drate = 1000; /* 1000 kb/sec */
        else if (disk_sects > 17)
            disk_drate = 500; /* 500 kb/sec */
        else if (disk_sects > 12)
            disk_drate = 300; /* 300 kb/sec */
        else
            disk_drate = 250; /* 250 kb/sec */

        apply_drate(fdc,drive,disk_drate);
    }

    if (disk_heads == 0) {
        unsigned char presp[4];
        unsigned int cnt=0;

        do_spin_up_motor(fdc,drive);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
 
        for (r_cyl=0;r_cyl < 20;r_cyl++) {
            if (do_read_sector_id(resp,fdc,1/*head 1 this time*/) >= 7 && (fdc->st[0] & 0xC0) == 0/*no error*/) {
                /*      3 |           Cylinder
                 *      4 |             Head
                 *      5 |        Sector Number
                 *      6 |         Sector Size */
                if (resp[4] == 1/*head*/ && resp[5] < 36 && resp[3] == 0/*track 0*/ && resp[6] == r_ssz/*same sector size*/) {
                    if (presp[0] != resp[3] || presp[1] != resp[4] |
                        presp[2] != resp[5] || presp[3] != resp[6]) {
                        presp[0] = resp[3];
                        presp[1] = resp[4];
                        presp[2] = resp[5];
                        presp[3] = resp[6];

                        cnt++;
                    }
                    else {
                        r_cyl--;
                    }
                }
            }
            else {
                r_cyl += 4 - 1;
            }
        }

        if (cnt >= 2)
            disk_heads = 2;
        else
            disk_heads = 1;

        fprintf(stderr,"Head autodetect: %u/20 valid reads on head 1. %u head format\n",cnt,disk_heads);
    }

    if (high_density_disk < 0 || high_density_drive < 0) {
        unsigned int cyl1=0,cyl2=0;

        /* try to autodetect high density drive vs double density floppy by seeking to Track 2.
         * if the logical track coming back is track 1, then this is a double density disk in a high density drive. */
        do_seek_drive(fdc,2);

        /* what do we see? */
        for (r_cyl=0;r_cyl < 10;r_cyl++) {
            if (do_read_sector_id(resp,fdc,0) >= 7 && (fdc->st[0] & 0xC0) == 0/*no error*/) {
                /*      3 |           Cylinder
                 *      4 |             Head
                 *      5 |        Sector Number
                 *      6 |         Sector Size */
                if (resp[4] == 0 && resp[5] < 36) {
                    if (resp[3] == 1) cyl1++;
                    else if (resp[3] == 2) cyl2++;
                }
            }
            else {
                r_cyl += 4 - 1;
            }
        }

        fprintf(stderr,"Autodetect: CYL1=%u CYL2=%u\n",cyl1,cyl2);

        if (cyl1 >= 2 || cyl2 >= 2) {
            /* having read physical track 2... */
            if (cyl2 >= (cyl1*3U)) { /* if Track 2 is most common, this is either: */
                                     /* - A high density disk in a high density drive, or
                                      * - A double density disk in a double density drive.
                                      *
                                      * It doesn't tell us if the floppy or drive are high density or not.
                                      * But since the track numbers match, it doesn't matter. */
            }
            else {                  /* if Track 1 is most common, this is a double density disk in a high density drive */
                if (high_density_disk < 0)
                    high_density_disk = 0; /* double density disk */
                if (high_density_drive < 0)
                    high_density_drive = 1; /* high density drive */
            }
        }
    }

    /* if we KNOW the drive is high density, and we KNOW the disk is double density,
     * then reading the disk requires seeking to TWICE the logical track number */
    track_2x = (high_density_drive > 0 && high_density_disk == 0) ? 2 : 1;

    /* guess for cyls */
    if (disk_cyls == 0) {
        if (track_2x == 2/*double density floppy in high density drive*/ ||
            high_density_drive == 0/* we KNOW it's a double density drive */)
            disk_cyls = 40;
    }

    /* some poking at the disk is required for more */
    if (disk_cyls == 0 && track_2x == 1) {
        /* a normal disk would have 40 tracks if double density, 80 if high density */
        unsigned char presp[4];
        unsigned int cnt=0;

        do_spin_up_motor(fdc,drive);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_calibrate_drive(fdc);
        do_seek_drive(fdc,41);
 
        for (r_cyl=0;r_cyl < 20;r_cyl++) {
            if (do_read_sector_id(resp,fdc,0) >= 7 && (fdc->st[0] & 0xC0) == 0/*no error*/) {
                /*      3 |           Cylinder
                 *      4 |             Head
                 *      5 |        Sector Number
                 *      6 |         Sector Size */
                if (resp[4] == 0/*head*/ && resp[5] < 36 && resp[3] == 41/*track 0*/ && resp[6] == r_ssz/*same sector size*/) {
                    if (presp[0] != resp[3] || presp[1] != resp[4] |
                        presp[2] != resp[5] || presp[3] != resp[6]) {
                        presp[0] = resp[3];
                        presp[1] = resp[4];
                        presp[2] = resp[5];
                        presp[3] = resp[6];

                        cnt++;
                    }
                    else {
                        r_cyl--;
                    }
                }
            }
            else {
                r_cyl += 4 - 1;
            }
        }

        if (cnt >= 2) {
            disk_cyls = 80;
            fprintf(stderr,"Cyl autodetect: %u tracks\n",cnt,disk_cyls);
        }
    }

    /* announce */
    printf("Disk geometry: CHS %u/%u/%u %u/sector hddisk=%d hddrive=%d ssz=%u(%ub)\n"
           "               trackx=%u drate=%u\n",
        disk_cyls,disk_heads,disk_sects,disk_bps,high_density_disk,high_density_drive,r_ssz,disk_bps,track_2x,disk_drate);

    /* we can't read high density floppies in a double density drive */
    if (high_density_disk > 0 && high_density_drive == 0) {
        fprintf(stderr,"That format requires a high density drive\n");
        return;
    }
    if (disk_cyls == 0 || disk_heads == 0 || disk_sects == 0 || disk_bps == 0) {
        fprintf(stderr,"Insufficient information\n");
        return;
    }

    img_fd = open("FLOPPY.DSK",O_RDWR|O_CREAT|O_TRUNC,0644);
    if (img_fd < 0) {
        fprintf(stderr,"Unable to open disk image\n");
        return;
    }

    do_spin_up_motor(fdc,drive);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    do_calibrate_drive(fdc);
    for (r_cyl=0;r_cyl < disk_cyls;r_cyl++) {
        printf("\x0d");
        printf("Seeking C=%3u... ",r_cyl * track_2x);
        fflush(stdout);

        do_spin_up_motor(fdc,drive);

        for (r_head=0;r_head < disk_heads;r_head++) {
            current_phys_head=r_head;
            do_spin_up_motor(fdc,drive);

            printf("\x0d");
            printf("Seeking C=%3u H=%u... ",r_cyl * track_2x,r_head);
            fflush(stdout);

            do_seek_drive(fdc,r_cyl * track_2x);

            for (r_sec=1;r_sec <= disk_sects;r_sec++) {
                int retry = 3*10;

do_retry:
                do_spin_up_motor(fdc,drive);
                data_length = disk_bps;

                if (kbhit()) {
                    int c = getch();

                    if (c == 27) {
                        r_head = 255;
                        r_sec = 255;
                        r_cyl = 255;
                        break;
                    }
                }

                printf("\x0D");
                printf("Reading C=%3u H=%u S=%03u... ",r_cyl,r_head,r_sec);
                fflush(stdout);

#if TARGET_MSDOS == 32
                memset(floppy_dma->lin,0,data_length);
#else
                _fmemset(floppy_dma->lin,0,data_length);
#endif

                if (retry-- == 0) {
                    fprintf(stderr,"GIVING UP\n");
                    goto do_write;
                }

                if ((retry%3) == 0) {
                    do_floppy_controller_reset(fdc);

                    if (disk_drate > 600)
                        floppy_controller_set_data_transfer_rate(fdc,3); /* 1000 */
                    else if (disk_drate > 400)
                        floppy_controller_set_data_transfer_rate(fdc,0); /* 500 */
                    else if (disk_drate > 280)
                        floppy_controller_set_data_transfer_rate(fdc,1); /* 300 */
                    else
                        floppy_controller_set_data_transfer_rate(fdc,2); /* 250 */

                    floppy_controller_set_motor_state(fdc,drive,0);
                    do_spin_up_motor(fdc,drive);

                    do_calibrate_drive(fdc);
                    do_calibrate_drive(fdc);
                    do_calibrate_drive(fdc);
                    do_calibrate_drive(fdc);
                    do_seek_drive(fdc,r_cyl * track_2x);
                    do_seek_drive(fdc,r_cyl * track_2x);
                }

                floppy_controller_read_status(fdc);
                if (!floppy_controller_can_write_data(fdc) || floppy_controller_busy_in_instruction(fdc))
                    do_floppy_controller_reset(fdc);

                {
                    outp(d8237_ioport(fdc->dma,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(fdc->dma) | D8237_MASK_SET); /* mask */

                    outp(d8237_ioport(fdc->dma,D8237_REG_W_WRITE_MODE),
                            D8237_MODER_CHANNEL(fdc->dma) |
                            D8237_MODER_TRANSFER(D8237_MODER_XFER_WRITE) |
                            D8237_MODER_MODESEL(D8237_MODER_MODESEL_SINGLE));

                    d8237_write_count(fdc->dma,data_length);
                    d8237_write_base(fdc->dma,floppy_dma->phys);

                    outp(d8237_ioport(fdc->dma,D8237_REG_W_SINGLE_MASK),D8237_MASK_CHANNEL(fdc->dma)); /* unmask */

                    inp(d8237_ioport(fdc->dma,D8237_REG_R_STATUS)); /* read status port to clear TC bits */
                }

                /* Read Sector (x6h)
                 *
                 *   Byte |  7   6   5   4   3   2   1   0
                 *   -----+---------------------------------
                 *      0 | MT  MF  SK   0   0   1   1   0
                 *      1 |  x   x   x   x   x  HD DR1 DR0
                 *      2 |            Cylinder
                 *      3 |              Head
                 *      4 |         Sector Number
                 *      5 |          Sector Size
                 *      6 |    Track length/last sector
                 *      7 |        Length of GAP3
                 *      8 |          Data Length
                 *
                 *         MT = Multi-track
                 *         MF = MFM/FM
                 *         SK = Skip deleted address mark
                 *         HD = Head select (on PC platform, doesn't matter)
                 *    DR1,DR0 = Drive select */

                /* NTS: To ensure we read only one sector, Track length/last sector must equal
                 * Sector Number field. The floppy controller stops reading on error or when
                 * sector number matches. This is very important for the PIO mode of this code,
                 * else we'll never complete reading properly and never get to reading back
                 * the status bytes. */

                wdo = 9;
                cmd[0] = /*no multitrack*/ + 0x40/* MFM */ + 0x06/* READ DATA */;
                cmd[1] = (fdc->digital_out&3)+(current_phys_head&1?0x04:0x00)/* [1:0] = DR1,DR0 [2:2] = HD */;
                cmd[2] = r_cyl;	/* cyl=0 */
                cmd[3] = r_head;/* head=0 */
                cmd[4] = r_sec;	/* sector=1 */
                cmd[5] = r_ssz;	/* sector size=2 (512 bytes) */
                cmd[6] = r_sec; /* last sector of the track (what sector to stop at). */
                cmd[7] = 0x1B;//current_phys_rw_gap;
                cmd[8] = 0xFF;//current_sectsize_smaller; /* DTL (not used if 256 or larger) */
                wd = floppy_controller_write_data(fdc,cmd,wdo);
                if (wd < 2) {
                    fprintf(stderr,"Write data port failed\n");
                    do_floppy_controller_reset(fdc);
                    goto do_retry;
                }

                if (fdc->use_irq) floppy_controller_wait_irq(fdc,10000,1); /* 10 seconds */
                floppy_controller_wait_data_ready_ms(fdc,1000);

                rdo = 7;
                rd = floppy_controller_read_data(fdc,resp,rdo);
                if (rd < 1) {
                    fprintf(stderr,"Read data port failed\n");
                    do_floppy_controller_reset(fdc);
                    goto do_retry;
                }

                /* Read Sector (x6h) response
                 *
                 *   Byte |  7   6   5   4   3   2   1   0
                 *   -----+---------------------------------
                 *      0 |              ST0
                 *      1 |              ST1
                 *      2 |              ST2
                 *      3 |           Cylinder
                 *      4 |             Head
                 *      5 |        Sector Number
                 *      6 |         Sector Size
                 */

                /* the command SHOULD terminate */
                floppy_controller_wait_data_ready(fdc,20);
                if (!floppy_controller_wait_busy_in_instruction(fdc,1000))
                    do_floppy_controller_reset(fdc);

                /* accept ST0..ST2 from response and update */
                if (rd >= 3) {
                    fdc->st[0] = resp[0];
                    fdc->st[1] = resp[1];
                    fdc->st[2] = resp[2];
                }
                if (rd >= 4) {
                    fdc->cylinder = resp[3];
                }

                {
                    uint32_t dma_len = d8237_read_count(fdc->dma);
                    uint8_t status = inp(d8237_ioport(fdc->dma,D8237_REG_R_STATUS));

                    /* some DMA controllers reset the count back to the original value on terminal count.
                     * so if the DMA controller says terminal count, the true count is zero */
                    if (status&D8237_STATUS_TC(fdc->dma)) dma_len = 0;

                    if (dma_len > (uint32_t)data_length) dma_len = (uint32_t)data_length;
                    returned_length = (unsigned int)((uint32_t)data_length - dma_len);
                }

                if (returned_length != data_length) {
                    fprintf(stderr,"Read DMA error (want=%u got=%u)\n",data_length,returned_length);
                    fprintf(stderr,"Resp: %02x %02x %02x %02x %02x %02x %02x\n",
                        resp[0],resp[1],resp[2],resp[3],
                        resp[4],resp[5],resp[6]);

                    goto do_retry;
                }

do_write:
                if (_dos_xwrite(img_fd,floppy_dma->lin,data_length) != data_length) {
                    printf("Error\n");
                    r_head = 255;
                    r_sec = 255;
                    r_cyl = 255;
                    break;
                }
            }
        }
    }
    printf("\n");

    close(img_fd);
}

static void help(void) {
    fprintf(stderr,"test [options]\n");
    fprintf(stderr," -h --help     Show this help\n");
    fprintf(stderr," -1            First controller only\n");
    fprintf(stderr," -2            Look for 2nd controller\n");
    fprintf(stderr,"These options control the density of the drive, not the media.\n");
    fprintf(stderr,"These options are REQUIRED for 5.25\" drives to read 360KB and 1.2MB properly.\n");
    fprintf(stderr," -hd           Drive is high density (1.2MB, 1.44MB)\n");
    fprintf(stderr," -dd           Drive is double density (360KB)\n");
    fprintf(stderr," -hdisk        Disk is high density\n");
    fprintf(stderr," -ddisk        Disk is double density\n");
    fprintf(stderr," -chs c/h/s    Geometry\n");
    fprintf(stderr," -bs n         Bytes per sector\n");
    fprintf(stderr," -br n         Data rate (250, 300, 500, 1000)\n");
    fprintf(stderr," -fmt <preset> Format preset\n");
    fprintf(stderr,"                   1.44 = 1.44MB HD\n");
    fprintf(stderr,"                   1.2 = 1.2MB HD\n");
    fprintf(stderr,"                   720 = 720KB DD\n");
    fprintf(stderr,"                   360 = 360KB DD\n");
    fprintf(stderr,"                   180 = 180KB DD single sided\n");
    fprintf(stderr,"                   1.2pc98 = 1.2MB HD PC-98\n");
    fprintf(stderr,"  WARNING: The ability to read PC-98 3-mode floppies may vary\n");
    fprintf(stderr,"           depending on your hardware and floppy controller.\n");
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
            else if (!strcmp(a,"2")) {
                floppy_controllers_enable_2nd = 1;
            }
            else if (!strcmp(a,"fmt")) {
                a = argv[i++];

                if (!strcmp(a,"1.44")) {
                    disk_bps = 512;
                    disk_cyls = 80;
                    disk_heads = 2;
                    disk_sects = 18;
                    high_density_disk = 1;
                    disk_drate = 500;
                }
                else if (!strcmp(a,"1.2")) {
                    disk_bps = 512;
                    disk_cyls = 80;
                    disk_heads = 2;
                    disk_sects = 15;
                    high_density_disk = 1;
                    disk_drate = 300; /* GUESS */
                }
                else if (!strcmp(a,"1.2pc98")) {
                    /* WARNING: Your IBM PC's ability to read PC-98 3-mode 1.2MB depends on the hardware
                     *          and floppy controller. Some can, some can't */
                    disk_bps = 1024;
                    disk_cyls = 77;
                    disk_heads = 2;
                    disk_sects = 8;
                    high_density_disk = 1;
                    disk_drate = 500;
                }
                else if (!strcmp(a,"720")) {
                    disk_bps = 512;
                    disk_cyls = 80;
                    disk_heads = 2;
                    disk_sects = 9;
                    high_density_disk = 1;
                    disk_drate = 250;
                }
                else if (!strcmp(a,"360")) {
                    disk_bps = 512;
                    disk_cyls = 40;
                    disk_heads = 2;
                    disk_sects = 9;
                    high_density_disk = 0;
                    disk_drate = 300;
                }
                else if (!strcmp(a,"180")) {
                    disk_bps = 512;
                    disk_cyls = 40;
                    disk_heads = 1;
                    disk_sects = 9;
                    high_density_disk = 0;
                    disk_drate = 300;
                }
                else {
                    fprintf(stderr,"Unknown preset\n");
                    return 1;
                }
            }
            else if (!strcmp(a,"br")) {
                a = argv[i++];
                if (a == NULL) return 1;
                disk_drate = strtoul(a,&a,10);
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
	do_floppy_controller_enable_irq(floppy,1);
	floppy_controller_enable_dma(floppy,1);

    /* do the read */
    do_read(floppy,drive);

    /* switch off motor */
    floppy_controller_set_motor_state(floppy,0,0);
    floppy_controller_set_motor_state(floppy,1,0);
    floppy_controller_set_motor_state(floppy,2,0);
    floppy_controller_set_motor_state(floppy,3,0);

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

