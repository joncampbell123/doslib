/* reset.c
 *
 * Test program: Various methods of resetting the system
 * (C) 2011-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 */

/* NOTES: Most motherboards do the reset recovery thing quite well, and despite
          the age of the 286-style reset vector, most modern motherboards apparently
	  support it. I have a Dell Optiplex Pentium III based system that emulates
	  it quite well.

          Pentium III based Dell Optiplex:
	     Supports all reset methods. 286 reset vector recovery works, though
	     apparently it's not a full on reset because it doesn't seem to re-
	     enable the Processor Serial Number. The piix:h reset option does not
	     work in conjunction with the 286 reset vector, but it DOES reset the
	     machine. The 32-bit build doesn't seem to be able to cause a soft
	     reset via piix:s, apparently that only works from 16-bit real mode (?)
 */

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
#include <hw/8042/8042.h>
#include <hw/8254/8254.h>

enum {
	RESET_ANY=0,		/* try any reset method */
	RESET_92h,		/* reset via port 0x92 */
	RESET_KEYBOARD,		/* reset via keyboard controller */
	RESET_PIIX_SOFT,	/* reset via port 0xCF9 (Intel PIIX soft-reset) */
	RESET_PIIX_HARD,	/* reset via port 0xCF9 (Intel PIIX hard-reset) */
	RESET_8086_ENTRY,	/* soft "reset" real-mode jump to 0xFFFF:0x0000 */
	RESET_TRIPLE_FAULT	/* reset by intentionally causing a triple fault */
};

enum {
	RECOV_IBM_286_BIOS=0
};

static void help() {
	fprintf(stderr,"reset [options]\n");
	fprintf(stderr,"Demonstrates causing a CPU reset by various methods\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"    /92         Use port 92h (recommended)\n");
	fprintf(stderr,"    /k          Use keyboard controller\n");
	fprintf(stderr,"    /piix:s     Intel PIIX port CF9h (soft)\n");
	fprintf(stderr,"    /piix:h     Intel PIIX port CF9h (hard)\n");
	fprintf(stderr,"    /8086       8086-style jump to BIOS entry point\n");
	fprintf(stderr,"    /tf         Cause CPU triple fault\n");
	fprintf(stderr,"    /any        Try all methods listed above\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"If this program is successful, the next screen you will see\n");
	fprintf(stderr,"will be your BIOS counting RAM and whatever else it does on startup.\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Optionally, you can also request that this program try one of several\n");
	fprintf(stderr,"methods of resuming control of the CPU after reset, instead of actually\n");
	fprintf(stderr,"causing the whole system to reboot.\n");
	fprintf(stderr,"    /rc:286     IBM 286 BIOS reset vector recovery\n");
	fprintf(stderr,"\n");
	fprintf(stderr,"Override switches---USE AT YOUR OWN RISK:\n");
	fprintf(stderr,"    /rc:v86     Force recovery testing even if virtual 8086 mode is active\n");
}

void reset_92h() {
	unsigned char c;

	/* this is pretty easy: just set bit 0 to 1.
	   it's said to have originated on IBM PS/2 hardware.
	   virtually every emulator and motherboard since
	   1993 recognizes this in hardware as well, and you
	   will also find the infamous A20 gate can be controlled
	   from here as well. */
	/* But then again, I do have one Pentium motherboard (1995-ish)
	   that doesn't have a port 0x92 and must be reset through the
	   keyboard controller or the PIIX reset register. */
	/* NOTE: Some motherboards have hardware bugs where sometimes
	         it can be already set, and setting has no effect, we
		 must CLEAR THEN SET. Especially Dell brand computers... */
	c = inp(0x92);
	outp(0x92,c & 0xFE);
	c = inp(0x92);
	outp(0x92,c | 1);
}

void reset_keyboard() {
	unsigned char op=0x32;/* reasonable assumption */
	int i;

	k8042_drain_buffer();

	/* read the output byte from the keyboard controller */
	k8042_write_command(0xAD);
	k8042_drain_buffer();

	if (k8042_write_command(0xD0)) {
		i = k8042_read_output_wait();
		if (i >= 0) op = (unsigned char)i | 3;
	}

	k8042_write_command(0xAE);
	k8042_drain_buffer();

	/* now write it back with bit 0 clear to reset the CPU */
	for (i=0;i < 20;i++) {
		k8042_write_command(0xD1);	/* write output port */
		k8042_write_data(op & 0xFE);	/* reset CPU */
		k8042_drain_buffer();
	}

	/* are we still here? try pulsing commands */
	for (i=0;i < 20;i++) {
		k8042_write_command(0xFE);
		k8042_drain_buffer();
	}
}

void reset_piix_soft() {
	/* Intel PIIX/PIIX3 motherboards have their own reset register
	   at 0xCF9 (smack-dab in the middle of the PCI configuration
	   registers) which can be used to cause a soft or hard reset */
	outp(0xCF9,0);
	outp(0xCF9,4); /* soft reset, BIOS controlled */
}

void reset_piix_hard() {
	/* Intel PIIX/PIIX3 motherboards have their own reset register
	   at 0xCF9 (smack-dab in the middle of the PCI configuration
	   registers) which can be used to cause a soft or hard reset */
	outp(0xCF9,0);
	outp(0xCF9,6); /* hard reset */
}

void reset_triple_fault() {
	/* TODO */
}

void reset_8086_entry();

jmp_buf recjmp;

#if TARGET_MSDOS == 16
void bios_reset_cb_e();
void bios_reset_cb() {
	*((unsigned short far*)MK_FP(0xB800,0x0002)) = 0x1F31;
	longjmp(recjmp,1);
}
#endif

int main(int argc,char **argv) {
#if TARGET_MSDOS == 16
	unsigned char pic_a_mask,pic_b_mask;
#endif
	int recovery = -1;
	int rec_v86 = 0;
	int how = -1;
	int keyb = 0;
	int i;

	if (argc < 2) {
		help();
		return 1;
	}

	for (i=1;i < argc;) {
		const char *a = argv[i++];

		if (*a == '/' || *a == '-') {
			do { a++; } while (*a == '/' || *a == '-');

			if (!strcmp(a,"92")) {
				how = RESET_92h;
			}
			else if (!strcmp(a,"k")) {
				how = RESET_KEYBOARD;
			}
			else if (!strcmp(a,"piix:s")) {
				how = RESET_PIIX_SOFT;
			}
			else if (!strcmp(a,"piix:h")) {
				how = RESET_PIIX_HARD;
			}
			else if (!strcmp(a,"8086")) {
				how = RESET_8086_ENTRY;
			}
			else if (!strcmp(a,"tf")) {
				how = RESET_TRIPLE_FAULT;
			}
			else if (!strcmp(a,"any")) {
				how = RESET_ANY;
			}
			else if (!strcmp(a,"rc:286")) {
				recovery = RECOV_IBM_286_BIOS;
			}
			else if (!strcmp(a,"rc:v86")) {
				rec_v86 = 1;
			}
			else {
				help();
				return 1;
			}
		}
		else {
			help();
			return 1;
		}
	}

	if (how < 0) {
		help();
		return 1;
	}

#if TARGET_MSDOS == 32
	if (how == RESET_8086_ENTRY) {
		printf("8086 entry point reset not supported in 32-bit builds\n");
		return 1;
	}
	if (recovery >= 0) {
		printf("Recovery testing not supported in 32-bit builds\n");
		return 1;
	}
#endif

	if (!probe_8254()) {
		printf("Cannot init 8254 timer\n");
		return 1;
	}
	if (!(keyb=k8042_probe()))
		printf("Warning: unable to probe 8042 controller. Is it there?\n");

	cpu_probe();
	printf("Your CPU is basically a %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));
	if (cpu_v86_active)
		printf(" - Your CPU is currently running me in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_FPU)
		printf(" - With FPU\n");

#if TARGET_MSDOS == 16
	/* apparently this trick will work when EMM386.EXE is active,
	   and it seems to be a very effective way for 16-bit DOS
	   programs to escape it's virtual 8086 mode, BUT, it also
	   seems to cause subtle memory corruption issues that
	   cause erratic behavior and eventually a crash. So it's
	   best not to allow it.

	   I have no idea what this would cause if run under any other
	   virtual 8086 like environment, but I'm pretty sure this would
	   cause a nasty crash if we did this from within a Windows
	   DOS Box!

	   TODO: See if it is possible to eventually find out what the
	         minor issues are, and whether this code can do some
		 extra wizardry to compensate. */
	if (cpu_v86_active && !rec_v86 && recovery >= RECOV_IBM_286_BIOS) {
		printf("ERROR: Recovery mode testing is NOT a good idea when virtual 8086\n");
		printf("       mode is active, it causes too many subtle and bizarre erratic\n");
		printf("       problems with the surrounding DOS environment.\n");
		printf("\n");
		printf("       To use this mode, disable v8086 mode and try again. If EMM386.EXE\n");
		printf("       is resident, you can disable v8086 mode by typing 'EMM386 OFF'\n");
		return 1;
	}

	if (recovery == RECOV_IBM_286_BIOS) {
		unsigned short segs=0,segp=0,segds=0;

		__asm {
			mov	ax,ss
			mov	segs,ax
			mov	ax,sp
			mov	segp,ax
			mov	ax,ds
			mov	segds,ax
		}

		pic_a_mask = inp(0x21);
		pic_b_mask = inp(0xA1);
		
		*((unsigned short far*)MK_FP(0x40,0x67)) = FP_OFF(bios_reset_cb_e);
		*((unsigned short far*)MK_FP(0x40,0x69)) = FP_SEG(bios_reset_cb_e);
		/* store DS somewhere where our reset routine can find it */
		*((unsigned short far*)MK_FP(0x50,0x04)) = FP_OFF(bios_reset_cb);
		*((unsigned short far*)MK_FP(0x50,0x06)) = FP_SEG(bios_reset_cb);
		*((unsigned short far*)MK_FP(0x50,0x08)) = segp;
		*((unsigned short far*)MK_FP(0x50,0x0A)) = segs;
		*((unsigned short far*)MK_FP(0x50,0x0C)) = segds;
		/* now write the CMOS to tell the BIOS what we're doing */
		outp(0x70,0x0F);
		outp(0x71,0x0A);
	}
#endif

	_cli();
	if (setjmp(recjmp)) {
/* --> CPU execution will resume here longjmp() style if reset vector called */
		printf("Recovery successful\n");
	}
	else {
		if (how == RESET_ANY) {
			reset_92h();
			t8254_wait(t8254_us2ticks(250000));
			reset_keyboard();
			t8254_wait(t8254_us2ticks(250000));
			reset_piix_soft();
			t8254_wait(t8254_us2ticks(250000));
			reset_piix_hard();
			t8254_wait(t8254_us2ticks(250000));
			reset_triple_fault();
			t8254_wait(t8254_us2ticks(250000));
#if TARGET_MSDOS == 16
			reset_8086_entry(); /* <- unless on ancient hardware, this is least likely to work */
#endif
		}
		else if (how == RESET_92h) {
			reset_92h();
		}
		else if (how == RESET_KEYBOARD) {
			reset_keyboard();
		}
		else if (how == RESET_PIIX_SOFT) {
			reset_piix_soft();
		}
		else if (how == RESET_PIIX_HARD) {
			reset_piix_hard();
		}
		else if (how == RESET_TRIPLE_FAULT) {
			reset_triple_fault();
		}
#if TARGET_MSDOS == 16
		else if (how == RESET_8086_ENTRY) {
			reset_8086_entry();
		}
#endif

		/* We need a small pause here, some ancient 386 motherboards
		   I own actually have quite a delay on them between reset
		   and actually causing the CPU to reset, enough that on a
		   very old OS I wrote, I had to stick a HLT instruction to
		   prevent the CPU from running wild through my kernel during
		   the 100-250ms it took for reset to actually happen. */
		for (i=0;i < 3;i++)
			t8254_wait(t8254_us2ticks(1000000));

		printf("* IF YOU CAN SEE THIS MESSAGE, RESET FAILED *\n");
	}

#if TARGET_MSDOS == 16
	if (recovery == RECOV_IBM_286_BIOS) {
		/* clear the CMOS flag */
		outp(0x70,0x0F);
		outp(0x71,0x00);
		/* restore the PIC mask */
		outp(0x21,pic_a_mask);
		outp(0xA1,pic_b_mask);
		/* depending on the BIOS, we may be responsible for reenabling the keyboard
		   and turning the clock lines back on */
		if (how == RESET_KEYBOARD) {
			/* read the output byte from the keyboard controller */
			k8042_write_command(0xAD);
			k8042_drain_buffer();

			/* now write it back with bit 0 clear to reset the CPU */
			for (i=0;i < 20;i++) {
				k8042_write_command(0xD1);	/* write output port */
				k8042_write_data(0x33);		/* keep A20 on, CPU on, disable inhibition */
				k8042_drain_buffer();
			}

			k8042_write_command(0xAE);
			k8042_drain_buffer();
		}
	}
#endif

	return 0;
}

