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
#include <dos.h>

#include <hw/vga/vga.h>
#include <hw/dos/dos.h>
#include <hw/8254/8254.h>		/* 8254 timer */
#include <hw/8259/8259.h>
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/adlib/adlib.h>

struct midi_channel {
	unsigned char		note_number;
	unsigned char		note_velocity;
	unsigned int		note_on:1;
};

struct midi_track {
	/* track data, raw */
	unsigned char*		raw;		/* raw data base */
	unsigned char*		fence;		/* raw data end (last byte + 1) */
	unsigned char*		read;		/* raw data read ptr */
	/* state */
	unsigned long		us_per_quarter_note; /* Microseconds per quarter note (def 120 BPM) */
	unsigned long		us_tick_cnt_mtpq; /* Microseconds advanced (up to 10000 us or one unit at 100Hz) x ticks per quarter note */
	unsigned long		wait;
	unsigned char		last_status;	/* MIDI last status byte */
};

#define MIDI_MAX_CHANNELS	16
#define MIDI_MAX_TRACKS		64

struct midi_channel		midi_ch[MIDI_MAX_CHANNELS];
struct midi_track		midi_trk[MIDI_MAX_TRACKS];
static unsigned int		midi_trk_count=0;
static unsigned char		midi_playing=0;

/* MIDI params. Nobody ever said it was a straightforward standard!
 * NTS: These are for reading reference. Internally we convert everything to 100Hz time base. */
static unsigned int ticks_per_quarter_note=0;	/* "Ticks per beat" */

static unsigned int irq0_cnt=0,irq0_add=0,irq0_max=0;
static void (interrupt *old_irq0)();
static unsigned long irq0_ticks=0;

static inline unsigned char midi_trk_read(struct midi_track *t) {
	unsigned char c;

	if (t->read == NULL || t->read >= t->fence) return 0xFF;
	c = *(t->read);
	t->read++;
	return c;
}

static inline void on_key_on(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char vel) {
	unsigned int ach = (unsigned int)(t - midi_trk); /* pointer math */
	double freq = 440 + key; /* FIXME */

#if 1
	fprintf(stderr,"on ach=%u\n",ach);
#endif

	if (ch->note_on) {
		adlib_fm[ach].mod.key_on = 0;
		adlib_update_groupA0(ach,&adlib_fm[ach]);
	}

	ch->note_on = 1;
	ch->note_number = key;
	ch->note_velocity = vel;

	adlib_freq_to_fm_op(&adlib_fm[ach].mod,freq);
	adlib_fm[ach].mod.key_on = 1;
	adlib_update_groupA0(ach,&adlib_fm[ach]);
}

static inline void on_key_off(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char vel) {
	unsigned int ach = (unsigned int)(t - midi_trk); /* pointer math */
	double freq = 440 + key; /* FIXME */

#if 1
	fprintf(stderr,"off ach=%u\n",ach);
#endif

	ch->note_on = 0;
	ch->note_number = key;
	ch->note_velocity = vel;

	adlib_freq_to_fm_op(&adlib_fm[ach].mod,freq);
	adlib_fm[ach].mod.key_on = 0;
	adlib_update_groupA0(ach,&adlib_fm[ach]);
}

static inline void on_control_change(struct midi_track *t,struct midi_channel *ch,unsigned char num,unsigned char val) {
}

static inline void on_program_change(struct midi_track *t,struct midi_channel *ch,unsigned char inst) {
}

static inline void on_pitch_bend(struct midi_track *t,struct midi_channel *ch,int bend/*-8192 to 8192*/) {
}

void midi_trk_end(struct midi_track *t) {
	t->wait = ~0UL;
	t->read = t->fence;
}

void midi_trk_skip(struct midi_track *t,unsigned long len) {
	unsigned long rem;

	if (t->read == NULL || t->read >= t->fence) return;
	if (len > 0xFFF0UL) {
		midi_trk_end(t);
		return;
	}
	rem = (unsigned long)(t->fence - t->read);
	if (len > rem) len = rem;
	t->read += len;
}

unsigned long midi_trk_read_delta(struct midi_track *t) {
	unsigned long tc = 0;
	unsigned char c = 0,b;

	if (t->read == NULL) return 0;
	while (c < 4 && t->read < t->fence) {
		b = *(t->read); (t->read)++;
		tc = (tc << 7UL) + (unsigned long)(b&0x7F);
		if (!(b&0x80)) break;
		c++;
	}

	return tc;
}

void midi_tick_track(unsigned int i) {
	struct midi_track *t = midi_trk + i;
	struct midi_channel *ch;
	unsigned char b,c,d;

	/*DEBUG*/
//	if (i != 0) return;
	if (t->read >= t->fence) return;

	t->us_tick_cnt_mtpq += 10000UL * (unsigned long)ticks_per_quarter_note;
	while (t->us_tick_cnt_mtpq >= t->us_per_quarter_note) {
		t->us_tick_cnt_mtpq -= t->us_per_quarter_note;
		if (t->wait == 0) {
			if (t->read >= t->fence) break;

#if 0
			fprintf(stderr,"Parse[%u]: ",i);
			{
				unsigned int i;
				for (i=0;i < 22 && (t->read+i) < t->fence;i++) fprintf(stderr,"%02x ",t->read[i]);
			}
			fprintf(stderr,"\n");
#endif

			/* read pointer should be pointing at MIDI event bytes, just after the time delay */
			b = midi_trk_read(t);
			if (b&0x80) {
				t->last_status = b;
				c = midi_trk_read(t);
			}
			else {
				/* blegh. last status */
				c = b;
				b = t->last_status;
			}
			switch (b>>4) {
				case 0x8: { /* note off */
					d = midi_trk_read(t);
					ch = midi_ch + (b&0xF); /* c=key d=velocity */
					on_key_off(t,ch,c,d);
					} break;
				case 0x9: { /* note on */
					d = midi_trk_read(t);
					ch = midi_ch + (b&0xF); /* c=key d=velocity */
					on_key_on(t,ch,c,d);
					} break;
				case 0xB: { /* control change */
					d = midi_trk_read(t);
					ch = midi_ch + (b&0xF); /* c=key d=velocity */
					on_control_change(t,ch,c,d);
					} break;
				case 0xC: { /* program change */
					on_program_change(t,ch,c); /* c=instrument d=not used */
					} break;
				case 0xE: { /* pitch bend */
					d = midi_trk_read(t);
					on_pitch_bend(t,ch,((c&0x7F)|((d&0x7F)<<7))-8192); /* c=LSB d=MSB */
					} break;
				case 0xF: { /* event */
					if (b == 0xFF) {
						if (c == 0x7F) { /* c=type d=len */
							unsigned long len = midi_trk_read_delta(t);
							fprintf(stderr,"Type 0x7F len=%lu %p/%p/%p\n",len,t->raw,t->read,t->fence);
							if (len < 512UL) {
								/* unknown */
								midi_trk_skip(t,len);
							}
							else {
								midi_trk_end(t);
							}
						}
						else if (c < 0x7F) {
							d = midi_trk_read(t);
							fprintf(stderr,"Type 0x%02x len=%lu %p/%p/%p\n",c,d,t->raw,t->read,t->fence);
							midi_trk_skip(t,d);
						}
						else {
							fprintf(stderr,"t=%u Unknown MIDI f message 0x%02x 0x%02x %p/%p/%p\n",i,b,c,t->raw,t->read,t->fence);
						}
					}
					else {
						unsigned long len = midi_trk_read_delta(t);
						fprintf(stderr,"Sysex len=%lu %p/%p/%p\n",len,d,t->raw,t->read,t->fence);
						midi_trk_skip(t,d);
					}
					} break;
				default:
					fprintf(stderr,"t=%u Unknown MIDI message 0x%02x at %p/%p/%p\n",i,b,t->raw,t->read,t->fence);
					midi_trk_end(t);
					break;
			};

#if 0
			fprintf(stderr,"Par-t[%u]: ",i);
			{
				unsigned int i;
				for (i=0;i < 22 && (t->read+i) < t->fence;i++) fprintf(stderr,"%02x ",t->read[i]);
			}
			fprintf(stderr,"\n");
#endif

			/* and then read the next event */
			t->wait = midi_trk_read_delta(t);
#if 0
			fprintf(stderr,"wait %lu\n",t->wait);
#endif
		}
		else {
			t->wait--;
		}
	}
}

void midi_tick() {
	unsigned int i;

	if (!midi_playing) return;
	for (i=0;i < midi_trk_count;i++) midi_tick_track(i);
}

/* WARNING: subroutine call in interrupt handler. make sure you compile with -zu flag for large/compact memory models */
void interrupt irq0() {
//	midi_tick();
	irq0_ticks++;
	if ((irq0_cnt += irq0_add) >= irq0_max) {
		irq0_cnt -= irq0_max;
		old_irq0();
	}
	else {
		p8259_OCW2(0,P8259_OCW2_NON_SPECIFIC_EOI);
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
		f->decay_rate = 2;
		f->sustain_level = 0;
		f->release_rate = 4;
		f->f_number = 400;
		f->octave = 4;
		f->key_on = 0;

		f = &adlib_fm[i].car;
		f->mod_multiple = 1;
		f->total_level = 63 - 16;
		f->attack_rate = 15;
		f->decay_rate = 2;
		f->sustain_level = 0;
		f->release_rate = 4;
		f->f_number = 0;
		f->octave = 0;
		f->key_on = 0;
	}

	adlib_apply_all();
}

void midi_reset_track(unsigned int i) {
	struct midi_track *t;

	if (i >= MIDI_MAX_TRACKS) return;
	t = &midi_trk[i];
	t->last_status = 0;
	t->us_tick_cnt_mtpq = 0;
	t->us_per_quarter_note = (60000000UL / 120UL); /* 120BPM */
	t->read = midi_trk[i].raw;
	t->wait = midi_trk_read_delta(t); /* and then the read pointer will point at the MIDI event when wait counts down */
}

void midi_reset_tracks() {
	int i;

	for (i=0;i < midi_trk_count;i++)
		midi_reset_track(i);
}

void midi_reset_channels() {
	int i;

	for (i=0;i < MIDI_MAX_CHANNELS;i++) {
		midi_ch[i].note_number = 69;
		midi_ch[i].note_velocity = 127;
		midi_ch[i].note_on = 0;
	}
}

int load_midi_file(const char *path) {
	unsigned char tmp[256];
	unsigned int tracks=0;
	unsigned int tracki=0;
	int fd;

	fd = open(path,O_RDONLY|O_BINARY);
	if (fd < 0) {
		printf("Failed to load file %s\n",path);
		return 0;
	}

	ticks_per_quarter_note = 0;
	while (read(fd,tmp,8) == 8) {
		uint32_t sz;

		sz =	((uint32_t)tmp[4] << (uint32_t)24) |
			((uint32_t)tmp[5] << (uint32_t)16) |
			((uint32_t)tmp[6] << (uint32_t)8) |
			((uint32_t)tmp[7] << (uint32_t)0);
		if (!memcmp(tmp,"MThd",4)) {
			unsigned short t,tdiv;

			if (sz < 6 || sz > 255) {
				fprintf(stderr,"Invalid MThd size %lu\n",(unsigned long)sz);
				goto err;
			}
			if (read(fd,tmp,(int)sz) != (int)sz) {
				fprintf(stderr,"MThd read error\n");
				goto err;
			}

			/* byte 0-1 = format type (0,1 or 2) */
			/* byte 2-3 = number of tracks */
			/* byte 4-5 = time divison */
			t = tmp[1] | (tmp[0] << 8);
			if (t > 1) {
				fprintf(stderr,"MThd type %u not supported\n",t);
				goto err; /* we only take type 0 or 1, don't support 2 */
			}
			tracks = tmp[3] | (tmp[2] << 8);
			if (tracks > MIDI_MAX_TRACKS) {
				fprintf(stderr,"MThd too many (%u) tracks\n",tracks);
				goto err;
			}
			tdiv = tmp[5] | (tmp[4] << 8);
			if (tdiv & 0x8000) {
				fprintf(stderr,"MThd SMPTE time division not supported\n");
				goto err; /* we do not support the SMPTE form */
			}
			if (tdiv == 0) {
				fprintf(stderr,"MThd time division == 0\n");
				goto err;
			}
			ticks_per_quarter_note = tdiv;
		}
		else if (!memcmp(tmp,"MTrk",4)) {
			if (sz == 0UL) continue;
			if (sz > 64000UL) goto err;
			if (tracki >= MIDI_MAX_TRACKS) goto err;
			midi_trk[tracki].raw = malloc(sz);
			if (midi_trk[tracki].raw == NULL) goto err;
			midi_trk[tracki].fence = midi_trk[tracki].raw + (unsigned)sz;
			midi_trk[tracki].read = midi_trk[tracki].raw;
			if (read(fd,midi_trk[tracki].raw,(unsigned)sz) != (int)sz) goto err;
			tracki++;
		}
		else {
			fprintf(stderr,"Unknown MIDI chunk %c%c%c%c\n",tmp[0],tmp[1],tmp[2],tmp[3]);
			goto err;
		}
	}
	if (tracki == 0 || ticks_per_quarter_note == 0) goto err;
	midi_trk_count = tracki;

	fprintf(stderr,"Ticks per quarter note: %u\n",ticks_per_quarter_note);

	close(fd);
	return 1;
err:
	close(fd);
	return 0;
}

int main(int argc,char **argv) {
	unsigned long ptick;
	int i,c;

	printf("ADLIB FM test program\n");
	if (argc < 2) {
		printf("You must specify a MIDI file to play\n");
		return 1;
	}

	if (!probe_vga()) {
		printf("Cannot init VGA\n");
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

	for (i=0;i < MIDI_MAX_TRACKS;i++) {
		midi_trk[i].raw = NULL;
		midi_trk[i].read = NULL;
		midi_trk[i].fence = NULL;
	}

	if (load_midi_file(argv[1]) == 0) {
		printf("Failed to load MIDI\n");
		return 1;
	}

	write_8254_system_timer(T8254_REF_CLOCK_HZ / 100); /* tick faster at 100Hz please */
	irq0_cnt = 0;
	irq0_add = 182;
	irq0_max = 1000; /* about 18.2Hz */
	old_irq0 = _dos_getvect(8);/*IRQ0*/
	_dos_setvect(8,irq0);

	adlib_shut_up();
	midi_reset_channels();
	midi_reset_tracks();
	_cli();
	irq0_ticks = ptick = 0;
	_sti();
	midi_playing = 1;

	while (1) {
		unsigned long adv;

		_cli();
		adv = irq0_ticks - ptick;
		if (adv >= 100UL) adv = 100UL;
		ptick = irq0_ticks;
		_sti();

		while (adv != 0) {
			midi_tick();
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

	midi_playing = 0;
	adlib_shut_up();
	shutdown_adlib();
	_dos_setvect(8,old_irq0);
	write_8254_system_timer(0); /* back to normal 18.2Hz */

	for (i=0;i < MIDI_MAX_TRACKS;i++) {
		if (midi_trk[i].raw) {
			free(midi_trk[i].raw);
			midi_trk[i].raw = NULL;
		}
		midi_trk[i].fence = NULL;
		midi_trk[i].read = NULL;
	}

	return 0;
}

