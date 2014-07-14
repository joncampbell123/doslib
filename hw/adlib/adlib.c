/* adlib.c
 *
 * Adlib OPL2/OPL3 FM synthesizer chipset controller library.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box]
 *
 * On most Sound Blaster compatible cards all the way up to the late 1990s, a
 * Yamaha OPL2 or OPL3 chipset exists (or may be emulated on PCI cards) that
 * responds to ports 388h-389h. Through these I/O ports you control the FM
 * synthesizer engine. On some cards, a second OPL2 may exist at 38A-38Bh,
 * and on ISA PnP cards, the OPL3 may be located at 38C-38Dh if software
 * configured. */
/* TODO: ISA PnP complementary library */
/* TODO: Modifications to the library to support OPL2/OPL3 chipsets at I/O ports
 *       other than 388h */
 
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/adlib/adlib.h>

unsigned short			adlib_voice_to_op_opl2[9] = {0x00,0x01,0x02, 0x08,0x09,0x0A, 0x10,0x11,0x12};
/* NTS: There is a HOWTO out there stating that the registers line up 0,1,2,6,7,8,... == WRONG! */
unsigned short			adlib_voice_to_op_opl3[18] = {0x00,0x01,0x02, 0x08,0x09,0x0A, 0x10,0x11,0x12, 0x100,0x101,0x102, 0x108,0x109,0x10A, 0x110,0x111,0x112};
unsigned short*			adlib_voice_to_op = adlib_voice_to_op_opl2;

struct adlib_reg_bd		adlib_reg_bd;
struct adlib_fm_channel		adlib_fm[ADLIB_FM_VOICES];
int				adlib_fm_voices = 0;
unsigned char			adlib_flags = 0;

struct adlib_fm_channel adlib_fm_preset_violin_opl3 = {
	.mod = {0,	1,	1,	1,	1,	1,	42,	6,	1,	1,	4,	0,
		3,	456,	1,	1,	1,	1,	4,	0,	5},
	.car = {0,	1,	1,	1,	1,	1,	63,	4,	1,	1,	4,	0,
		3,	456,	1,	1,	1,	1,	0,	0,	2}
};

struct adlib_fm_channel adlib_fm_preset_violin_opl2 = {
	.mod = {0,	1,	1,	1,	1,	1,	42,	6,	1,	1,	4,	0,
		3,	456,	1,	1,	1,	1,	2,	0,	1},
	.car = {0,	1,	1,	1,	1,	1,	63,	4,	1,	1,	4,	0,
		3,	456,	1,	1,	1,	1,	0,	0,	2}
};

struct adlib_fm_channel adlib_fm_preset_piano = {
	.mod = {0,	0,	1,	1,	1,	1,	42,	10,	4,	2,	3,	0,
		4,	456,	1,	1,	1,	1,	4,	0,	0},
	.car = {0,	0,	1,	1,	1,	1,	63,	10,	1,	8,	3,	0,
		4,	456,	1,	1,	1,	1,	0,	0,	0}
};

struct adlib_fm_channel adlib_fm_preset_harpsichord = {
	.mod = {0,	0,	1,	1,	1,	1,	42,	10,	3,	2,	3,	0,
		4,	456,	1,	1,	1,	1,	2,	0,	3},
	.car = {0,	0,	1,	1,	1,	1,	63,	10,	5,	3,	3,	0,
		4,	456,	1,	1,	1,	1,	0,	0,	3}
};

/* NTS: adjust the modulator total level value to vary between muted (27) and open (47) with
 *      further adjustment if you want to mimick the change in sound when you blow harder */
struct adlib_fm_channel adlib_fm_preset_horn = {
	.mod = {0,	0,	1,	0,	1,	0,	47,	6,	1,	1,	7,	0,
		4,	514,	1,	1,	1,	1,	0,	0,	0},
	.car = {0,	0,	1,	0,	1,	0,	47,	8,	2,	2,	7,	0,
		4,	456,	1,	1,	1,	1,	0,	0,	0}
};

struct adlib_fm_channel adlib_fm_preset_deep_bass_drum = {
	.mod = {0,	0,	0,	0,	1,	0,	13,	7,	1,	0,	1,	0,
		2,	456,	1,	1,	1,	1,	7,	0,	1},
	.car = {0,	0,	1,	1,	1,	1,	63,	15,	2,	6,	1,	0,
		2,	456,	1,	1,	1,	1,	0,	0,	0}
};

/* NTS: You can simulate hitting software or harder by adjusting the modulator total volume
 *      as well as raising or lowering the frequency */
struct adlib_fm_channel adlib_fm_preset_small_drum = {
	.mod = {0,	0,	0,	1,	1,	1,	54,	15,	10,	15,	15,	0,
		3,	456,	1,	1,	1,	1,	1,	0,	0},
	.car = {0,	0,	1,	1,	1,	1,	63,	15,	7,	15,	15,	0,
		3,	456,	1,	1,	1,	1,	1,	0,	0}
};

unsigned char adlib_read(unsigned short i) {
	unsigned char c;
	outp(ADLIB_IO_INDEX+((i>>8)*2),(unsigned char)i);
	adlib_wait();
	c = inp(ADLIB_IO_DATA+((i>>8)*2));
	adlib_wait();
	return c;
}

void adlib_write(unsigned short i,unsigned char d) {
	outp(ADLIB_IO_INDEX+((i>>8)*2),(unsigned char)i);
	adlib_wait();
	outp(ADLIB_IO_DATA+((i>>8)*2),d);
	adlib_wait();
}

/* TODO: adlib_write_imm_1() and adlib_write_imm_2()
 *       this would allow DOS programs to use this ADLIB library from within
 *       an interrupt routine */

int probe_adlib(unsigned char sec) {
	unsigned char a,b,retry=3;
	unsigned short bas = sec ? 0x100 : 0;

	/* this code uses the 8254 for timing */
	if (!probe_8254())
		return 1;

	do {
		adlib_write(0x04+bas,0x60);			/* reset both timers */
		adlib_write(0x04+bas,0x80);			/* enable interrupts */
		a = adlib_status(sec);
		adlib_write(0x02+bas,0xFF);			/* timer 1 */
		adlib_write(0x04+bas,0x21);			/* start timer 1 */
		t8254_wait(t8254_us2ticks(100));
		b = adlib_status(sec);
		adlib_write(0x04+bas,0x60);			/* reset both timers */
		adlib_write(0x04+bas,0x00);			/* disable interrupts */

		if ((a&0xE0) == 0x00 && (b&0xE0) == 0xC0)
			return 1;

	} while (--retry != 0);

	return 0;
}

int init_adlib() {
	adlib_flags = 0;
	if (!probe_adlib(0))
		return 0;

	adlib_write(0x01,0x20);	/* enable waveform select */
	adlib_voice_to_op = adlib_voice_to_op_opl2;
	adlib_fm_voices = 9;

	if (probe_adlib(1)) {
		adlib_fm_voices = 18;
		adlib_flags = ADLIB_FM_DUAL_OPL2;
	}
	else {
		/* NTS: "unofficial" method of detecting OPL3 */
		if ((adlib_status(0) & 0x06) == 0) {
			adlib_fm_voices = 18;
			adlib_flags = ADLIB_FM_OPL3;
			adlib_voice_to_op = adlib_voice_to_op_opl3;

			/* init like an OPL3 */
			adlib_write(0x105,0x01);		/* set OPL3 bit */
			probe_adlib(0);
			adlib_write(0x104,0x00);		/* disable any 4op connections */
		}
	}

	return 1;
}

void shutdown_adlib_opl3() {
	if (adlib_flags & ADLIB_FM_OPL3) {
		adlib_write(0x105,0x00);		/* clear OPL3 bit */
		probe_adlib(0);
		adlib_fm_voices = 9;
		adlib_voice_to_op = adlib_voice_to_op_opl2;
		adlib_flags &= ~ADLIB_FM_OPL3;
	}
}

void shutdown_adlib() {
	shutdown_adlib_opl3();
}

void adlib_update_group20(unsigned int op,struct adlib_fm_operator *f) {
	adlib_write(0x20+op,	(f->am << 7) |
				(f->vibrato << 6) |
				(f->sustain << 5) |
				(f->key_scaling_rate << 4) |
				(f->mod_multiple << 0));
}

void adlib_update_group40(unsigned int op,struct adlib_fm_operator *f) {
	adlib_write(0x40+op,	(f->level_key_scale << 6) |
				((f->total_level^63) << 0));
}

void adlib_update_group60(unsigned int op,struct adlib_fm_operator *f) {
	adlib_write(0x60+op,	(f->attack_rate << 4) |
				(f->decay_rate << 0));
}

void adlib_update_group80(unsigned int op,struct adlib_fm_operator *f) {
	adlib_write(0x80+op,	(f->sustain_level << 4) |
				(f->release_rate << 0));
}

void adlib_update_groupA0(unsigned int channel,struct adlib_fm_channel *ch) {
	struct adlib_fm_operator *f = &ch->mod;
	unsigned int x = (channel >= 9) ? 0x100 : 0;
	adlib_write(0xA0+(channel%9)+x,	 f->f_number);
	adlib_write(0xB0+(channel%9)+x,	(f->key_on << 5) |
					(f->octave << 2) |
					(f->f_number >> 8));
}

void adlib_update_groupC0(unsigned int channel,struct adlib_fm_channel *ch) {
	struct adlib_fm_operator *f = &ch->mod;
	unsigned int x = (channel >= 9) ? 0x100 : 0;
	adlib_write(0xC0+(channel%9)+x,	(f->feedback << 1) |
					(f->connection << 0) |
					(f->ch_d << 7) |
					(f->ch_c << 6) |
					(f->ch_b << 5) |
					(f->ch_a << 4));
}

void adlib_update_groupE0(unsigned int op,struct adlib_fm_operator *f) {
	adlib_write(0xE0+op,	(f->waveform << 0));
}

void adlib_update_operator(unsigned int op,struct adlib_fm_operator *f) {
	adlib_update_group20(op,f);
	adlib_update_group40(op,f);
	adlib_update_group60(op,f);
	adlib_update_group80(op,f);
	adlib_update_groupE0(op,f);
}

void adlib_update_bd(struct adlib_reg_bd *b) {
	adlib_write(0xBD,	(b->am_depth << 7) |
				(b->vibrato_depth << 6) |
				(b->rythm_enable << 5) |
				(b->bass_drum_on << 4) |
				(b->snare_drum_on << 3) |
				(b->tom_tom_on << 2) |
				(b->cymbal_on << 1) |
				(b->hi_hat_on << 0));
}

void adlib_apply_all() {
	struct adlib_fm_operator *f;
	unsigned short op;
	int ch;

	for (ch=0;ch < adlib_fm_voices;ch++) {
		f = &adlib_fm[ch].mod; op = adlib_voice_to_op[ch];   adlib_update_operator(op,f);
		f = &adlib_fm[ch].car; op = adlib_voice_to_op[ch]+3; adlib_update_operator(op,f);
		adlib_update_groupA0(ch,&adlib_fm[ch]);
		adlib_update_groupC0(ch,&adlib_fm[ch]);
	}
	adlib_update_bd(&adlib_reg_bd);
}

double adlib_fm_op_to_freq(struct adlib_fm_operator *f) {
	unsigned long t = (unsigned long)f->f_number * 49716UL;
	return (double)t / (1UL << (20UL - (unsigned long)f->octave));
}

void adlib_freq_to_fm_op(struct adlib_fm_operator *f,double freq) {
	unsigned long l;

	freq *= (1UL << (20UL - (unsigned long)f->octave));
	l = (unsigned long)freq / 49716UL;
	f->octave = 4;
	while (l > 1023UL) {
		f->octave++;
		l >>= 1UL;
	}
	while (l != 0UL && l < 256UL) {
		f->octave--;
		l <<= 1UL;
	}
	f->f_number = l;
}

