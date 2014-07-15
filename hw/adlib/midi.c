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
#include <hw/vga/vgagui.h>
#include <hw/vga/vgatty.h>
#include <hw/adlib/adlib.h>

/* one per OPL channel */
struct midi_note {
	unsigned char		note_number;
	unsigned char		note_velocity;
	unsigned char		note_track;	/* from what MIDI track */
	unsigned char		note_channel;	/* from what MIDI channel */
	unsigned int		busy:1;		/* if occupied */
};

struct midi_channel {
	unsigned char		program;
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
	unsigned int		eof:1;		/* we hit the end of the track */
};

#define MIDI_MAX_CHANNELS	16
#define MIDI_MAX_TRACKS		64

struct midi_note		midi_notes[ADLIB_FM_VOICES];
struct midi_channel		midi_ch[MIDI_MAX_CHANNELS];
struct midi_track		midi_trk[MIDI_MAX_TRACKS];
static unsigned int		midi_trk_count=0;
static volatile unsigned char	midi_playing=0;

/* MIDI params. Nobody ever said it was a straightforward standard!
 * NTS: These are for reading reference. Internally we convert everything to 100Hz time base. */
static unsigned int ticks_per_quarter_note=0;	/* "Ticks per beat" */

static void (interrupt *old_irq0)();
static volatile unsigned long irq0_ticks=0;
static volatile unsigned int irq0_cnt=0,irq0_add=0,irq0_max=0;

#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
static inline unsigned long farptr2phys(unsigned char far *p) { /* take 16:16 pointer convert to physical memory address */
	return ((unsigned long)FP_SEG(p) << 4UL) + ((unsigned long)FP_OFF(p));
}
#endif

static inline unsigned char midi_trk_read(struct midi_track *t) {
	unsigned char c;

	/* NTS: 16-bit large/compact builds MUST compare pointers as unsigned long to compare FAR pointers correctly! */
	if (t->read == NULL || (unsigned long)t->read >= (unsigned long)t->fence) {
		t->eof = 1;
		return 0xFF;
	}

	c = *(t->read);
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
	if (FP_OFF(t->read) >= 0xF) /* 16:16 far pointer aware (NTS: Programs reassigning this pointer MUST normalize the FAR pointer) */
		t->read = MK_FP(FP_SEG(t->read)+0x1,0);
	else
		t->read++;
#else
	t->read++;
#endif
	return c;
}

void midi_trk_end(struct midi_track *t) {
	t->wait = ~0UL;
	t->read = t->fence;
}

void midi_trk_skip(struct midi_track *t,unsigned long len) {
	unsigned long rem;

	/* NTS: 16-bit large/compact builds MUST compare pointers as unsigned long to compare FAR pointers correctly! */
	if (t->read == NULL || (unsigned long)t->read >= (unsigned long)t->fence)
		return;

	if (len > 0xFFF0UL) {
		midi_trk_end(t);
		return;
	}
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
	{
		unsigned long tt;

		tt = farptr2phys(t->read);
		rem = farptr2phys(t->fence) - tt;
		if (rem > len) rem = len;
		tt += rem;
		t->read = MK_FP(tt>>4,tt&0xF);
	}
#else
	rem = (unsigned long)(t->fence - t->read);
	if (len > rem) len = rem;
	t->read += len;
#endif
}

static const uint32_t midikeys_freqs[0x80] = {
	0x00082d01,	/* key 0 = 8.17579891564371Hz */
	0x0008a976,	/* key 1 = 8.66195721802725Hz */
	0x00092d51,	/* key 2 = 9.17702399741899Hz */
	0x0009b904,	/* key 3 = 9.72271824131503Hz */
	0x000a4d05,	/* key 4 = 10.3008611535272Hz */
	0x000ae9d3,	/* key 5 = 10.9133822322814Hz */
	0x000b8ff4,	/* key 6 = 11.5623257097386Hz */
	0x000c3ff6,	/* key 7 = 12.2498573744297Hz */
	0x000cfa70,	/* key 8 = 12.9782717993733Hz */
	0x000dc000,	/* key 9 = 13.75Hz */
	0x000e914f,	/* key 10 = 14.5676175474403Hz */
	0x000f6f11,	/* key 11 = 15.4338531642539Hz */
	0x00105a02,	/* key 12 = 16.3515978312874Hz */
	0x001152ec,	/* key 13 = 17.3239144360545Hz */
	0x00125aa2,	/* key 14 = 18.354047994838Hz */
	0x00137208,	/* key 15 = 19.4454364826301Hz */
	0x00149a0a,	/* key 16 = 20.6017223070544Hz */
	0x0015d3a6,	/* key 17 = 21.8267644645627Hz */
	0x00171fe9,	/* key 18 = 23.1246514194771Hz */
	0x00187fed,	/* key 19 = 24.4997147488593Hz */
	0x0019f4e0,	/* key 20 = 25.9565435987466Hz */
	0x001b8000,	/* key 21 = 27.5Hz */
	0x001d229e,	/* key 22 = 29.1352350948806Hz */
	0x001ede22,	/* key 23 = 30.8677063285078Hz */
	0x0020b404,	/* key 24 = 32.7031956625748Hz */
	0x0022a5d8,	/* key 25 = 34.647828872109Hz */
	0x0024b545,	/* key 26 = 36.7080959896759Hz */
	0x0026e410,	/* key 27 = 38.8908729652601Hz */
	0x00293414,	/* key 28 = 41.2034446141087Hz */
	0x002ba74d,	/* key 29 = 43.6535289291255Hz */
	0x002e3fd2,	/* key 30 = 46.2493028389543Hz */
	0x0030ffda,	/* key 31 = 48.9994294977187Hz */
	0x0033e9c0,	/* key 32 = 51.9130871974931Hz */
	0x00370000,	/* key 33 = 55Hz */
	0x003a453d,	/* key 34 = 58.2704701897612Hz */
	0x003dbc44,	/* key 35 = 61.7354126570155Hz */
	0x00416809,	/* key 36 = 65.4063913251497Hz */
	0x00454bb0,	/* key 37 = 69.295657744218Hz */
	0x00496a8b,	/* key 38 = 73.4161919793519Hz */
	0x004dc820,	/* key 39 = 77.7817459305202Hz */
	0x00526829,	/* key 40 = 82.4068892282175Hz */
	0x00574e9b,	/* key 41 = 87.307057858251Hz */
	0x005c7fa4,	/* key 42 = 92.4986056779086Hz */
	0x0061ffb5,	/* key 43 = 97.9988589954373Hz */
	0x0067d380,	/* key 44 = 103.826174394986Hz */
	0x006e0000,	/* key 45 = 110Hz */
	0x00748a7b,	/* key 46 = 116.540940379522Hz */
	0x007b7888,	/* key 47 = 123.470825314031Hz */
	0x0082d012,	/* key 48 = 130.812782650299Hz */
	0x008a9760,	/* key 49 = 138.591315488436Hz */
	0x0092d517,	/* key 50 = 146.832383958704Hz */
	0x009b9041,	/* key 51 = 155.56349186104Hz */
	0x00a4d053,	/* key 52 = 164.813778456435Hz */
	0x00ae9d36,	/* key 53 = 174.614115716502Hz */
	0x00b8ff49,	/* key 54 = 184.997211355817Hz */
	0x00c3ff6a,	/* key 55 = 195.997717990875Hz */
	0x00cfa700,	/* key 56 = 207.652348789973Hz */
	0x00dc0000,	/* key 57 = 220Hz */
	0x00e914f6,	/* key 58 = 233.081880759045Hz */
	0x00f6f110,	/* key 59 = 246.941650628062Hz */
	0x0105a025,	/* key 60 = 261.625565300599Hz */
	0x01152ec0,	/* key 61 = 277.182630976872Hz */
	0x0125aa2e,	/* key 62 = 293.664767917408Hz */
	0x01372082,	/* key 63 = 311.126983722081Hz */
	0x0149a0a7,	/* key 64 = 329.62755691287Hz */
	0x015d3a6d,	/* key 65 = 349.228231433004Hz */
	0x0171fe92,	/* key 66 = 369.994422711634Hz */
	0x0187fed4,	/* key 67 = 391.995435981749Hz */
	0x019f4e00,	/* key 68 = 415.304697579945Hz */
	0x01b80000,	/* key 69 = 440Hz */
	0x01d229ec,	/* key 70 = 466.16376151809Hz */
	0x01ede220,	/* key 71 = 493.883301256124Hz */
	0x020b404a,	/* key 72 = 523.251130601197Hz */
	0x022a5d81,	/* key 73 = 554.365261953744Hz */
	0x024b545c,	/* key 74 = 587.329535834815Hz */
	0x026e4104,	/* key 75 = 622.253967444162Hz */
	0x0293414f,	/* key 76 = 659.25511382574Hz */
	0x02ba74da,	/* key 77 = 698.456462866008Hz */
	0x02e3fd24,	/* key 78 = 739.988845423269Hz */
	0x030ffda9,	/* key 79 = 783.990871963499Hz */
	0x033e9c01,	/* key 80 = 830.60939515989Hz */
	0x03700000,	/* key 81 = 880Hz */
	0x03a453d8,	/* key 82 = 932.32752303618Hz */
	0x03dbc440,	/* key 83 = 987.766602512248Hz */
	0x04168094,	/* key 84 = 1046.50226120239Hz */
	0x0454bb03,	/* key 85 = 1108.73052390749Hz */
	0x0496a8b8,	/* key 86 = 1174.65907166963Hz */
	0x04dc8208,	/* key 87 = 1244.50793488832Hz */
	0x0526829e,	/* key 88 = 1318.51022765148Hz */
	0x0574e9b5,	/* key 89 = 1396.91292573202Hz */
	0x05c7fa49,	/* key 90 = 1479.97769084654Hz */
	0x061ffb53,	/* key 91 = 1567.981743927Hz */
	0x067d3802,	/* key 92 = 1661.21879031978Hz */
	0x06e00000,	/* key 93 = 1760Hz */
	0x0748a7b1,	/* key 94 = 1864.65504607236Hz */
	0x07b78880,	/* key 95 = 1975.5332050245Hz */
	0x082d0128,	/* key 96 = 2093.00452240479Hz */
	0x08a97607,	/* key 97 = 2217.46104781498Hz */
	0x092d5171,	/* key 98 = 2349.31814333926Hz */
	0x09b90410,	/* key 99 = 2489.01586977665Hz */
	0x0a4d053c,	/* key 100 = 2637.02045530296Hz */
	0x0ae9d36b,	/* key 101 = 2793.82585146403Hz */
	0x0b8ff493,	/* key 102 = 2959.95538169308Hz */
	0x0c3ff6a7,	/* key 103 = 3135.96348785399Hz */
	0x0cfa7005,	/* key 104 = 3322.43758063956Hz */
	0x0dc00000,	/* key 105 = 3520Hz */
	0x0e914f62,	/* key 106 = 3729.31009214472Hz */
	0x0f6f1100,	/* key 107 = 3951.06641004899Hz */
	0x105a0250,	/* key 108 = 4186.00904480958Hz */
	0x1152ec0e,	/* key 109 = 4434.92209562995Hz */
	0x125aa2e3,	/* key 110 = 4698.63628667852Hz */
	0x13720820,	/* key 111 = 4978.03173955329Hz */
	0x149a0a79,	/* key 112 = 5274.04091060592Hz */
	0x15d3a6d6,	/* key 113 = 5587.65170292806Hz */
	0x171fe927,	/* key 114 = 5919.91076338615Hz */
	0x187fed4e,	/* key 115 = 6271.92697570799Hz */
	0x19f4e00a,	/* key 116 = 6644.87516127912Hz */
	0x1b800000,	/* key 117 = 7040Hz */
	0x1d229ec4,	/* key 118 = 7458.62018428944Hz */
	0x1ede2200,	/* key 119 = 7902.13282009799Hz */
	0x20b404a1,	/* key 120 = 8372.01808961916Hz */
	0x22a5d81c,	/* key 121 = 8869.84419125991Hz */
	0x24b545c7,	/* key 122 = 9397.27257335704Hz */
	0x26e41040,	/* key 123 = 9956.06347910659Hz */
	0x293414f2,	/* key 124 = 10548.0818212118Hz */
	0x2ba74dac,	/* key 125 = 11175.3034058561Hz */
	0x2e3fd24f,	/* key 126 = 11839.8215267723Hz */
	0x30ffda9c	/* key 127 = 12543.853951416Hz */
};

static uint32_t midi_note_freq(struct midi_channel *ch,unsigned char key) {
	return midikeys_freqs[key&0x7F];
}

static struct midi_note *get_fm_note(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char do_alloc) {
	unsigned int tch = (unsigned int)(t - midi_trk); /* pointer math */
	unsigned int ach = (unsigned int)(ch - midi_ch); /* pointer math */
	unsigned int i,freen=~0;

	for (i=0;i < ADLIB_FM_VOICES;i++) {
		if (midi_notes[i].busy) {
			if (midi_notes[i].note_channel == ach && midi_notes[i].note_track == tch && midi_notes[i].note_number == key)
				return &midi_notes[i];
		}
		else {
			if (freen == ~0) freen = i;
		}
	}

	if (do_alloc && freen != ~0) return &midi_notes[freen];
	return NULL;
}

static void drop_fm_note(struct midi_channel *ch,unsigned char key) {
	unsigned int ach = (unsigned int)(ch - midi_ch); /* pointer math */
	unsigned int i;

	for (i=0;i < ADLIB_FM_VOICES;i++) {
		if (midi_notes[i].busy && midi_notes[i].note_channel == ach) {
			midi_notes[i].busy = 0;
			break;
		}
	}
}

static inline void on_key_aftertouch(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char vel) {
	struct midi_note *note = get_fm_note(t,ch,key,/*do_alloc*/0);
	uint32_t freq = midi_note_freq(ch,key);
	unsigned int ach;

	if (note == NULL) return;

	note->busy = 1;
	note->note_number = key;
	note->note_velocity = vel;
	note->note_track = (unsigned int)(t - midi_trk);
	note->note_channel = (unsigned int)(ch - midi_ch);
	ach = (unsigned int)(note - midi_notes); /* which FM channel? */
	adlib_freq_to_fm_op(&adlib_fm[ach].mod,(double)freq / 65536);
	adlib_fm[ach].mod.attack_rate = vel >> 3; /* 0-127 to 0-15 */
	adlib_fm[ach].mod.sustain_level = vel >> 3;
	adlib_fm[ach].mod.key_on = 1;
	adlib_update_groupA0(ach,&adlib_fm[ach]);
}

static inline void on_key_on(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char vel) {
	struct midi_note *note = get_fm_note(t,ch,key,/*do_alloc*/1);
	uint32_t freq = midi_note_freq(ch,key);
	unsigned int ach;

	/* HACK: Ignore percussion */
	if ((ch->program >= 8 && ch->program <= 15)/*Chromatic percussion*/ ||
		(ch->program >= 112 && ch->program <= 119)/*Percussive*/ ||
		ch == &midi_ch[9]/*MIDI channel 10 (DAMN YOU 1-BASED COUNTING)*/)
		return;

	if (note == NULL) {
		/* then we'll have to knock one off to make room */
		drop_fm_note(ch,key);
		note = get_fm_note(t,ch,key,1);
		if (note == NULL) return;
	}

	note->busy = 1;
	note->note_number = key;
	note->note_velocity = vel;
	note->note_track = (unsigned int)(t - midi_trk);
	note->note_channel = (unsigned int)(ch - midi_ch);
	ach = (unsigned int)(note - midi_notes); /* which FM channel? */
	adlib_freq_to_fm_op(&adlib_fm[ach].mod,(double)freq / 65536);
	adlib_fm[ach].mod.attack_rate = vel >> 3; /* 0-127 to 0-15 */
	adlib_fm[ach].mod.sustain_level = vel >> 3;
	adlib_fm[ach].mod.key_on = 1;
	adlib_update_groupA0(ach,&adlib_fm[ach]);
}

static inline void on_key_off(struct midi_track *t,struct midi_channel *ch,unsigned char key,unsigned char vel) {
	struct midi_note *note = get_fm_note(t,ch,key,/*do_alloc*/0);
	uint32_t freq = midi_note_freq(ch,key);
	unsigned int ach;

	if (note == NULL) return;

	note->busy = 0;
	ach = (unsigned int)(note - midi_notes); /* which FM channel? */
	adlib_freq_to_fm_op(&adlib_fm[ach].mod,(double)freq / 65536);
	adlib_fm[ach].mod.attack_rate = vel >> 3; /* 0-127 to 0-15 */
	adlib_fm[ach].mod.sustain_level = vel >> 3;
	adlib_fm[ach].mod.key_on = 0;
	adlib_update_groupA0(ach,&adlib_fm[ach]);
}

static inline void on_control_change(struct midi_track *t,struct midi_channel *ch,unsigned char num,unsigned char val) {
}

static inline void on_program_change(struct midi_track *t,struct midi_channel *ch,unsigned char inst) {
	ch->program = inst;
}

static inline void on_channel_aftertouch(struct midi_track *t,struct midi_channel *ch,unsigned char velocity) {
}

static inline void on_pitch_bend(struct midi_track *t,struct midi_channel *ch,int bend/*-8192 to 8192*/) {
}

unsigned long midi_trk_read_delta(struct midi_track *t) {
	unsigned long tc = 0;
	unsigned char c = 0,b;

	/* NTS: 16-bit large/compact builds MUST compare pointers as unsigned long to compare FAR pointers correctly! */
	if (t->read == NULL || (unsigned long)t->read >= (unsigned long)t->fence)
		return tc;

	while (c < 4) {
		b = midi_trk_read(t);
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
	int cnt=0;

	/* NTS: 16-bit large/compact builds MUST compare pointers as unsigned long to compare FAR pointers correctly! */
	if (t->read == NULL || (unsigned long)t->read >= (unsigned long)t->fence) {
		t->eof = 1;
		return;
	}

	t->us_tick_cnt_mtpq += 10000UL * (unsigned long)ticks_per_quarter_note;
	while (t->us_tick_cnt_mtpq >= t->us_per_quarter_note) {
		t->us_tick_cnt_mtpq -= t->us_per_quarter_note;
		cnt++;

		while (t->wait == 0) {
			if ((unsigned long)t->read >= (unsigned long)t->fence) {
				t->eof = 1;
				break;
			}

			/* read pointer should be pointing at MIDI event bytes, just after the time delay */
			b = midi_trk_read(t);
			if (b&0x80) {
				if (b < 0xF8) {
					if (b >= 0xF0)
						t->last_status = 0;
					else
						t->last_status = b;
				}
				if (b != 0x00 && ((b&0xF8) != 0xF0))
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
					if (d != 0) on_key_on(t,ch,c,d); /* "A Note On with a velocity of 0 is actually a note off" Bleh, really? */
					else on_key_off(t,ch,c,d);
					} break;
				case 0xA: { /* polyphonic aftertouch */
					d = midi_trk_read(t);
					ch = midi_ch + (b&0xF); /* c=key d=velocity */
					on_key_aftertouch(t,ch,c,d);
					} break;
				case 0xB: { /* control change */
					d = midi_trk_read(t);
					ch = midi_ch + (b&0xF); /* c=key d=velocity */
					on_control_change(t,ch,c,d);
					} break;
				case 0xC: { /* program change */
					on_program_change(t,ch,c); /* c=instrument d=not used */
					} break;
				case 0xD: { /* channel aftertouch */
					on_channel_aftertouch(t,ch,c); /* c=velocity d=not used */
					} break;
				case 0xE: { /* pitch bend */
					d = midi_trk_read(t);
					on_pitch_bend(t,ch,((c&0x7F)|((d&0x7F)<<7))-8192); /* c=LSB d=MSB */
					} break;
				case 0xF: { /* event */
					if (b == 0xFF) {
						if (c == 0x7F) { /* c=type d=len */
							unsigned long len = midi_trk_read_delta(t);
//							fprintf(stderr,"Type 0x7F len=%lu %p/%p/%p\n",len,t->raw,t->read,t->fence);
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

							if (c == 0x51 && d >= 3) {
								d -= 3;
								t->us_per_quarter_note = ((unsigned long)midi_trk_read(t)<<16UL)+
									((unsigned long)midi_trk_read(t)<<8UL)+
									((unsigned long)midi_trk_read(t)<<0UL);

								if (1/*TODO: If format 0 or format 1*/) {
									/* Ugh. Unless format 2, the tempo applies to all tracks */
									int j;

									for (j=0;j < midi_trk_count;j++) {
										if (j != i) midi_trk[j].us_per_quarter_note =
											t->us_per_quarter_note;
									}
								}
							}
							else {
//								fprintf(stderr,"Type 0x%02x len=%lu %p/%p/%p\n",c,d,t->raw,t->read,t->fence);
							}

							midi_trk_skip(t,d);
						}
						else {
							fprintf(stderr,"t=%u Unknown MIDI f message 0x%02x 0x%02x %p/%p/%p\n",i,b,c,t->raw,t->read,t->fence);
						}
					}
					else {
						unsigned long len = midi_trk_read_delta(t);
//						fprintf(stderr,"Sysex len=%lu %p/%p/%p\n",len,t->raw,t->read,t->fence);
						midi_trk_skip(t,len);
					}
					} break;
				default:
					if (b != 0x00) {
						fprintf(stderr,"t=%u Unknown MIDI message 0x%02x at %p/%p/%p\n",i,b,t->raw,t->read,t->fence);
						midi_trk_end(t);
					}
					break;
			};

			/* and then read the next event */
			t->wait = midi_trk_read_delta(t);
		}
		if (t->wait != 0) {
			t->wait--;
		}
	}
}

void adlib_shut_up();
void midi_reset_tracks();
void midi_reset_channels();

void midi_tick() {
	if (midi_playing) {
		unsigned int i;
		int eof=0;

		for (i=0;i < midi_trk_count;i++) {
			midi_tick_track(i);
			eof += midi_trk[i].eof?1:0;
		}

		if (eof >= midi_trk_count) {
			adlib_shut_up();
			midi_reset_tracks();
			midi_reset_channels();
		}
	}
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

		midi_notes[i].busy = 0;
		midi_notes[i].note_channel = 0;

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

void midi_reset_track(unsigned int i) {
	struct midi_track *t;

	if (i >= MIDI_MAX_TRACKS) return;
	t = &midi_trk[i];
	t->eof = 0;
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
		midi_ch[i].program = 0;
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
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
			if (sz > (640UL << 10UL)) goto err; /* 640KB */
#elif TARGET_MSDOS == 32
			if (sz > (1UL << 20UL)) goto err; /* 1MB */
#else
			if (sz > (60UL << 10UL)) goto err; /* 60KB */
#endif
			if (tracki >= MIDI_MAX_TRACKS) goto err;
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
			{
				unsigned segv;

				/* NTS: _fmalloc() is still limited to 64KB sizes */
				if (_dos_allocmem((unsigned)((sz+15UL)>>4UL),&segv) != 0) goto err;
				midi_trk[tracki].raw = MK_FP(segv,0);
			}
#else
			midi_trk[tracki].raw = malloc(sz);
#endif
			if (midi_trk[tracki].raw == NULL) goto err;
			midi_trk[tracki].read = midi_trk[tracki].raw;
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
			{
				unsigned char far *p = midi_trk[tracki].raw;
				unsigned long rem = (unsigned long)sz;
				unsigned long cando;
				unsigned read;

				while (rem != 0UL) {
					read = 0;

					cando = 0x10000UL - (unsigned long)FP_OFF(p);
					if (cando > rem) cando = rem;
					if (cando > 0xFFFFUL) cando = 0xFFFFUL; /* we're limited to 64KB-1 of reading */

					if (_dos_read(fd,p,(unsigned)cando,&read) != 0) goto err;
					if (read != (unsigned)cando) goto err;

					rem -= cando;
					if ((((unsigned long)FP_OFF(p))+cando) == 0x10000UL)
						p = MK_FP(FP_SEG(p)+0x1000,0);
					else
						p += (unsigned)cando;
				}

				cando = farptr2phys(p);
				midi_trk[tracki].fence = MK_FP(cando>>4,cando&0xF);
			}
#else
			midi_trk[tracki].fence = midi_trk[tracki].raw + (unsigned)sz;
			if (read(fd,midi_trk[tracki].raw,(unsigned)sz) != (int)sz) goto err;
#endif
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
#if TARGET_MSDOS == 16 && (defined(__LARGE__) || defined(__COMPACT__))
			_dos_freemem(FP_SEG(midi_trk[i].raw)); /* NTS: Because we allocated with _dos_allocmem */
#else
			free(midi_trk[i].raw);
#endif
			midi_trk[i].raw = NULL;
		}
		midi_trk[i].fence = NULL;
		midi_trk[i].read = NULL;
	}

	return 0;
}

