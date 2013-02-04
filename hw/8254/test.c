/* test.c
 *
 * 8254 programmable interrupt timer test program.
 * (C) 2008-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */

/* NTS: As of 2011/02/27 the 8254 routines no longer do cli/sti for us, we are expected
 *      to do them ourself. This is for performance reasons as well as for sanity reasons
 *      should we ever need to use the subroutines from within an interrupt handler */

#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8254/8254.h>

static volatile unsigned int counter = 0;
static unsigned int speaker_rate = T8254_REF_CLOCK_HZ / 400;	/* 400Hz */
static unsigned int max = 0xFFFF;

void (__interrupt __far *prev_irq0)() = NULL;
static void __interrupt __far irq0() {
	counter++;
	outp(0x20,0x20);
}

#if TARGET_MSDOS == 32
/* 32-bit DOS flat mode doesn't impose restrictions */
unsigned char tmp[238617];
#else
/* we're probably limited by 16-bit real mode and near pointers */
unsigned char tmp[32768];
#endif

void pulse_width_test() {
	int fd;
	unsigned char cc;
	unsigned int play;
	unsigned char plan_b=0;
	unsigned int patience = 10000;

	_cli();
	write_8254_system_timer(0xFFFF);	/* BUGFIX: Personal experience tells me BIOSes would fail reading the floppy if the IRQ 0 timer isn't ticking along at 18Hz */
	_sti();

	fd = open("..\\test1_22.wav",O_RDONLY|O_BINARY);
	if (fd < 0) fd = open("test1_22.wav",O_RDONLY|O_BINARY);
	if (fd < 0) {
		fprintf(stderr,"Cannot open test WAV\n");
		return;
	}
	lseek(fd,44,SEEK_SET);
	read(fd,tmp,sizeof(tmp));
	for (play=0;play < sizeof(tmp);play++) tmp[play] = (((tmp[play]) * 53) / 255) + 1; /* add "noise" to dither */
	close(fd);

	/* set timer 0 to 54 ticks (1.191MHz / 54 ~ 22050Hz) */
	_cli();
	t8254_pc_speaker_set_gate(0);
	write_8254_pc_speaker(1);
	write_8254_system_timer(54);
	_sti();

	_cli();
	{
		outp(T8254_CONTROL_PORT,(0 << 6) | (0 << 4) | 0);	/* latch counter N, counter latch read */
		do {
			if (--patience == 1) break;
			cc = inp(T8254_TIMER_PORT(0));
			inp(T8254_TIMER_PORT(0));
		} while (cc < (54/2));
		do {
			if (--patience == 0) break;
			cc = inp(T8254_TIMER_PORT(0));
			inp(T8254_TIMER_PORT(0));
		} while (cc >= (54/2));
		if (patience <= 2) {
			write_8254_system_timer(0xFFFF);	/* BUGFIX: on very old slow machines, the 54-count tick can easily cause the printf() below to absolutely CRAWL... */
			_sti();
			fprintf(stderr,"Oops! Either your CPU is too fast or the timer countdown trick doesn't work.\n");
			_cli();
			write_8254_system_timer(54);
			plan_b = 1;
		}
	}
	_sti();

	while (1) {
		if (kbhit()) {
			int c = getch();
			if (c == 27) break;
		}

		if (plan_b) {
			/* run with interrupts enabled, use IRQ0 to know when to tick */
			_cli();
			counter = 0;
			t8254_pc_speaker_set_gate(3);
			for (play=0;play < sizeof(tmp);) {
				outp(T8254_CONTROL_PORT,(2 << 6) | (1 << 4) | (T8254_MODE_0_INT_ON_TERMINAL_COUNT << 1)); /* MODE 0, low byte only, counter 2 */
				outp(T8254_TIMER_PORT(2),tmp[play]);
				_sti();
				while (counter == 0);
				_cli();
				play += counter;
				counter = 0;
			}
			_sti();
		}
		else {
			_cli();
			t8254_pc_speaker_set_gate(3);
			outp(T8254_CONTROL_PORT,(0 << 6) | (0 << 4) | 0);	/* latch counter N, counter latch read */
			for (play=0;play < sizeof(tmp);play++) {
				outp(T8254_CONTROL_PORT,(2 << 6) | (1 << 4) | (T8254_MODE_0_INT_ON_TERMINAL_COUNT << 1)); /* MODE 0, low byte only, counter 2 */
				outp(T8254_TIMER_PORT(2),tmp[play]);

				do {
					cc = inp(T8254_TIMER_PORT(0));
					inp(T8254_TIMER_PORT(0));
				} while (cc < (54/2));

				do {
					cc = inp(T8254_TIMER_PORT(0));
					inp(T8254_TIMER_PORT(0));
				} while (cc >= (54/2));
			}
			_sti();
		}
	}

	t8254_pc_speaker_set_gate(0);
}

int main() {
	struct t8254_readback_t readback;
	t8254_time_t tick[3];
	unsigned int i;

	printf("8254 library test program\n");
	if (!probe_8254()) {
		printf("Chip not present. Your computer might be 2010-era hardware that dropped support for it.\n");
		return 1;
	}

	prev_irq0 = _dos_getvect(T8254_IRQ+0x08);
	_dos_setvect(T8254_IRQ+0x8,irq0);

	_cli();
	write_8254_pc_speaker(speaker_rate);
	write_8254_system_timer(max);
	_sti();

	while (1) {
		if (kbhit()) {
			int c = getch();
			if (c == 27)
				break;
			else if (c == '-') {
				max -= 80;
				if (max > (0xFFFF-80))
					max = 0xFFFF;

				_cli();
				write_8254_system_timer(max);
				_sti();
			}
			else if (c == '=') {
				max += 110;
				if (max < 110 || max > (0xFFFF-110))
					max = 0xFFFF;
	
				_cli();
				write_8254_system_timer(max);
				_sti();
			}
			/* play with timer 2 and the PC speaker gate */
			else if (c == 'p') {
				unsigned char on = (t8254_pc_speaker_read_gate() != 0) ? 1 : 0;
				if (on) t8254_pc_speaker_set_gate(0);
				else t8254_pc_speaker_set_gate(3);
			}
			else if (c == '[') {
				speaker_rate += 110;
				if (speaker_rate > (0xFFFF-110) || speaker_rate < 110)
					speaker_rate = 0xFFFF;

				write_8254_pc_speaker(speaker_rate);
			}
			else if (c == ']') {
				speaker_rate -= 110;
				if (speaker_rate > (0xFFFF-110))
					speaker_rate = 0;

				write_8254_pc_speaker(speaker_rate);
			}
			else if (c == 'w') {
				printf("\n");
				pulse_width_test();
				_cli();
				write_8254_system_timer(max);
				_sti();
				printf("\n");
			}
			else if (c == 'z') {
				/* sleep-wait loop test */
				unsigned long delay_ticks;
				unsigned long z;
				unsigned int c,cmax;

				printf("\nDelay interval in us? ");
				z = 1000000; scanf("%lu",&z);

				delay_ticks = t8254_us2ticks(z);
				printf("  %lu = %lu ticks\n",z,delay_ticks);

				if (delay_ticks == 0UL)	cmax = T8254_REF_CLOCK_HZ / 20;
				else			cmax = T8254_REF_CLOCK_HZ / 20 / delay_ticks;
				if (cmax == 0) cmax = 1;

				write_8254_pc_speaker(T8254_REF_CLOCK_HZ / 400); /* tick as fast as possible */
				while (1) {
					if (kbhit()) {
						if (getch() == 27) break;
					}

					for (c=0;c < cmax;c++) {
						t8254_pc_speaker_set_gate(3);
						t8254_wait(delay_ticks);
						t8254_pc_speaker_set_gate(0);
						t8254_wait(delay_ticks);
					}
				}
			}
			else if (c == 'd') {
				printf("\n                                    \nDetail mode, hit 'd' again to exit: [WARNING: 8254 only]\n");
				while (1) {
					if (kbhit()) {
						int c = getch();
						if (c == 'd') {
							break;
						}
					}

					_cli();
					readback_8254(T8254_READBACK_ALL,&readback);
					_sti();
					printf("\x0D");
					for (i=0;i <= 2;i++) {
						printf("[%u] stat=%02x count=%04x  ",i,
							readback.timer[i].status,
							readback.timer[i].count);
					}
					fflush(stdout);
				}
				printf("\n");
			}
		}

		for (i=0;i <= 2;i++) tick[i] = read_8254(i);
		printf("\x0D %04x %04x %04x max=%04x count=%04x",tick[0],tick[1],tick[2],max,counter);
		fflush(stdout);
	}
	printf("\n");

	_cli();
	write_8254_pc_speaker(0);
	t8254_pc_speaker_set_gate(0);
	_dos_setvect(T8254_IRQ+0x8,prev_irq0);
	_sti();

	write_8254_system_timer(0xFFFF); /* restore normal function to prevent BIOS from going crazy */
	return 0;
}

