/* adlib.h
 *
 * Adlib OPL2/OPL3 FM synthesizer chipset controller library.
 * (C) 2010-2012 Jonathan Campbell.
 * Hackipedia DOS library.
 *
 * This code is licensed under the LGPL.
 * <insert LGPL legal text here>
 *
 * Compiles for intended target environments:
 *   - MS-DOS [pure DOS mode, or Windows or OS/2 DOS Box] */
 
#include <hw/cpu/cpu.h>
#include <stdint.h>

#define ADLIB_FM_VOICES			18

#define ADLIB_IO_INDEX			0x388
#define ADLIB_IO_STATUS			0x388
#define ADLIB_IO_DATA			0x389

#define ADLIB_IO_INDEX2			0x38A
#define ADLIB_IO_STATUS2		0x38A
#define ADLIB_IO_DATA2			0x38B

/* Adlib status */
#define ADLIB_STATUS_TIMERS_EXPIRED	0x80
#define ADLIB_STATUS_TIMER1_EXPIRED	0x40
#define ADLIB_STATUS_TIMER2_EXPIRED	0x20

enum {
	ADLIB_FM_DUAL_OPL2=0x01,
	ADLIB_FM_OPL3=0x02
};

struct adlib_fm_operator {
	/* 0x20-0x3F */
	uint8_t			am:1;			/* bit 7: Apply amplitude modulation */
	uint8_t			vibrato:1;		/* bit 6: Apply vibrato */
	uint8_t			sustain:1;		/* bit 5: maintain sustain level */
	uint8_t			key_scaling_rate:1;	/* bit 4: increase ADSR enevelope speed as pitch increases */
	uint8_t			mod_multiple:4;		/* bits 0-3: modulator multiple (1=voice frequency, 2=one octave above) */
	/* 0x40-0x5F */
	uint8_t			level_key_scale:2;	/* bits 7-6: decrease volume as frequency rises (0=none 1=1.5dB/8ve 2=3dB/8ve 3=6dB/8ve) */
	uint8_t			total_level:6;		/* bits 5-0: total output level (for sanity's sake, we maintain here as 0=silent 0x3F=full even though hardware is opposite) */
	/* 0x60-0x7F */
	uint8_t			attack_rate:4;		/* bits 7-4: attack rate */
	uint8_t			decay_rate:4;		/* bits 3-0: decay rate */
	/* 0x80-0x9F */
	uint8_t			sustain_level:4;	/* bits 7-4: sustain level */
	uint8_t			release_rate:4;		/* bits 3-0: release rate */
	/* 0xA0-0xBF */
	uint16_t		key_on:1;		/* bit 5: voice the channel */
	uint16_t		octave:3;		/* bits 4-2: octave */
	uint16_t		f_number:10;		/* bits 1-0, then bits 7-0: F-number (frequency) */
	/* 0xC0-0xCF */
	uint8_t			ch_a:1;			/* bit 4: OPL3: Channel A output */
	uint8_t			ch_b:1;			/* bit 5: OPL3: Channel B output */
	uint8_t			ch_c:1;			/* bit 6: OPL3: Channel C output */
	uint8_t			ch_d:1;			/* bit 7: OPL3: Channel D output */
	uint8_t			feedback:3;		/* bits 3-1: feedback strength */
	uint8_t			connection:1;		/* bit 0: connection (operator 1 and 2 independent if set) */
	/* 0xE0-0xFF */
	uint8_t			waveform:3;		/* bits 1-0: which waveform to use */
};

struct adlib_fm_channel {
	struct adlib_fm_operator	mod,car;
};

struct adlib_reg_bd {
	uint8_t			am_depth:1;
	uint8_t			vibrato_depth:1;
	uint8_t			rythm_enable:1;
	uint8_t			bass_drum_on:1;
	uint8_t			snare_drum_on:1;
	uint8_t			tom_tom_on:1;
	uint8_t			cymbal_on:1;
	uint8_t			hi_hat_on:1;
};

int init_adlib();
void shutdown_adlib();
void shutdown_adlib_opl3();
int probe_adlib(unsigned char sec);
unsigned char adlib_read(unsigned short i);
void adlib_write(unsigned short i,unsigned char d);
void adlib_update_group20(unsigned int op,struct adlib_fm_operator *f);
void adlib_update_group40(unsigned int op,struct adlib_fm_operator *f);
void adlib_update_group60(unsigned int op,struct adlib_fm_operator *f);
void adlib_update_group80(unsigned int op,struct adlib_fm_operator *f);
void adlib_update_groupA0(unsigned int channel,struct adlib_fm_channel *ch);
void adlib_update_groupC0(unsigned int channel,struct adlib_fm_channel *ch);
void adlib_update_groupE0(unsigned int op,struct adlib_fm_operator *f);
void adlib_update_operator(unsigned int op,struct adlib_fm_operator *f);
void adlib_freq_to_fm_op(struct adlib_fm_operator *f,double freq);
double adlib_fm_op_to_freq(struct adlib_fm_operator *f);
void adlib_update_bd(struct adlib_reg_bd *b);
void adlib_apply_all();

extern unsigned short			adlib_voice_to_op_opl2[9];
extern unsigned short			adlib_voice_to_op_opl3[18];
extern unsigned short*			adlib_voice_to_op;

extern struct adlib_reg_bd		adlib_reg_bd;
extern struct adlib_fm_channel		adlib_fm[ADLIB_FM_VOICES];
extern int				adlib_fm_voices;
extern unsigned char			adlib_flags;

extern struct adlib_fm_channel		adlib_fm_preset_deep_bass_drum;
extern struct adlib_fm_channel		adlib_fm_preset_violin_opl3;
extern struct adlib_fm_channel		adlib_fm_preset_violin_opl2;
extern struct adlib_fm_channel		adlib_fm_preset_harpsichord;
extern struct adlib_fm_channel		adlib_fm_preset_small_drum;
extern struct adlib_fm_channel		adlib_fm_preset_piano;
extern struct adlib_fm_channel		adlib_fm_preset_horn;

/* NTS: I have a Creative CT1350B card where we really do have to wait at least
 *      33us per I/O access, because the OPL2 chip on it really is that slow.
 *      
 *      Peior to this fix, the adlib code would often fail on a real CT1350B
 *      (unless run just after the Sound Blaster test program) and even if it
 *      did run, only about 1/3rd of the voices would work. Upping the delay
 *      to 40us for OPL3 and 100us for OPL2 resolved these issues. */
static inline void adlib_wait() {
	t8254_wait(t8254_us2ticks((adlib_flags & ADLIB_FM_OPL3) ? 40 : 100));
}

static inline unsigned char adlib_status(unsigned char which) {
	adlib_wait();
	return inp(ADLIB_IO_STATUS+(which*2));
}

static inline unsigned char adlib_status_imm(unsigned char which) {
	return inp(ADLIB_IO_STATUS+(which*2));
}

