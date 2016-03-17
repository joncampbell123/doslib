/* midi.c
 *
 * Adlib OPL2/OPL3 FM synthesizer chipset test program.
 * Play MIDI file using the OPLx synthesizer (well, poorly anyway)
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 */
 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>
#include <hw/adlib/adlib.h>

static void (interrupt *old_irq0)();
static volatile unsigned long irq0_ticks=0;
static volatile unsigned int irq0_cnt=0,irq0_add=0,irq0_max=0;

#pragma pack(push,1)
struct imf_entry {
	uint8_t		reg,data;
	uint16_t	delay;
};
#pragma pack(pop)

static struct imf_entry*	imf_music=NULL;
static struct imf_entry*	imf_play_ptr=NULL;
static struct imf_entry*	imf_music_end=NULL;
static uint16_t			imf_delay_countdown=0;

void imf_free_music() {
	if (imf_music) free(imf_music);
	imf_music = imf_play_ptr = imf_music_end = NULL;
	imf_delay_countdown = 0;
}

int imf_load_music(const char *path) {
	unsigned char buf[8];
	unsigned long len;
	int fd;

	imf_free_music();

	fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) return 0;

	len = lseek(fd,0,SEEK_END);
	lseek(fd,0,SEEK_SET);
	read(fd,buf,2);
	if (buf[0] != 0 || buf[1] != 0) // type 1 IMF
		len = *((uint16_t*)buf);
	else
		lseek(fd,0,SEEK_SET);

	if (len == 0 || len > 65535UL) {
		close(fd);
		return 0;
	}
	len -= len & 3;

	imf_music = malloc(len);
	if (imf_music == NULL) {
		close(fd);
		return 0;
	}
	read(fd,imf_music,len);
	close(fd);

	imf_play_ptr = imf_music;
	imf_music_end = imf_music + (len >> 2UL);
	return 1;
}

/* WARNING: subroutine call in interrupt handler. make sure you compile with -zu flag for large/compact memory models */
void interrupt irq0() {
	irq0_ticks++;
	if ((irq0_cnt += irq0_add) >= irq0_max) {
		irq0_cnt -= irq0_max;
		old_irq0();
	}
	else {
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
	}
}

void imf_tick() {
	if (imf_delay_countdown == 0) {
		do {
			adlib_write(imf_play_ptr->reg,imf_play_ptr->data);
			imf_delay_countdown = imf_play_ptr->delay;
			imf_play_ptr++;
			if (imf_play_ptr == imf_music_end)
				imf_play_ptr = imf_music;
		} while (imf_delay_countdown == 0);
	}
	else {
		imf_delay_countdown--;
	}
}

void adlib_shut_up() {
	int i;

	memset(adlib_fm,0,sizeof(adlib_fm));
	memset(&adlib_reg_bd,0,sizeof(adlib_reg_bd));
	for (i=0;i < adlib_fm_voices;i++) {
		struct adlib_fm_operator *f;
		f = &adlib_fm[i].mod;
		f->ch_a = f->ch_b = f->ch_c = f->ch_d = 1;
		f = &adlib_fm[i].car;
		f->ch_a = f->ch_b = f->ch_c = f->ch_d = 1;
	}

	for (i=0;i < adlib_fm_voices;i++) {
		struct adlib_fm_operator *f;

		f = &adlib_fm[i].mod;
		f->mod_multiple = 1;
		f->total_level = 63 - 16;
		f->attack_rate = 15;
		f->decay_rate = 4;
		f->sustain_level = 0;
		f->release_rate = 8;
		f->f_number = 400;
		f->sustain = 1;
		f->octave = 4;
		f->key_on = 0;

		f = &adlib_fm[i].car;
		f->mod_multiple = 1;
		f->total_level = 63 - 16;
		f->attack_rate = 15;
		f->decay_rate = 4;
		f->sustain_level = 0;
		f->release_rate = 8;
		f->f_number = 0;
		f->sustain = 1;
		f->octave = 0;
		f->key_on = 0;
	}

	adlib_apply_all();
}

int main(int argc,char **argv) {
	unsigned long tickrate = 700;
	unsigned long ptick;
	int c;

	printf("ADLIB FM test program IMFPLAY\n");
	if (argc < 2) {
		printf("You must specify an IMF file to play\n");
		return 1;
	}

	if (!init_adlib()) {
		printf("Cannot init library\n");
		return 1;
	}
	if (!probe_8254()) { /* we need the timer to keep time with the music */
		printf("8254 timer not found\n");
		return 1;
	}

	if (!imf_load_music(argv[1])) {
		printf("Failed to load IMF Music\n");
		return 1;
	}

	write_8254_system_timer(T8254_REF_CLOCK_HZ / tickrate);
	irq0_cnt = 0;
	irq0_add = 182;
	irq0_max = 1000; /* about 18.2Hz */
	old_irq0 = _dos_getvect(8);/*IRQ0*/
	_dos_setvect(8,irq0);

	adlib_shut_up();
	shutdown_adlib_opl3(); // NTS: Apparently the music won't play otherwise
	_cli();
	irq0_ticks = ptick = 0;
	_sti();

	while (1) {
		unsigned long adv;

		_cli();
		adv = irq0_ticks - ptick;
		if (adv >= 100UL) adv = 100UL;
		ptick = irq0_ticks;
		_sti();

		while (adv != 0) {
			imf_tick();
			adv--;
		}

		if (kbhit()) {
			c = getch();
			if (c == 0) c = getch() << 8;

			if (c == 27) {
				break;
			}
		}
	}

	imf_free_music();
	adlib_shut_up();
	shutdown_adlib();
	_dos_setvect(8,old_irq0);
	write_8254_system_timer(0); /* back to normal 18.2Hz */
	return 0;
}

