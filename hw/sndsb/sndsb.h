/* WARNING: As usual for performance reasons this library generally does not
 *          enable/disable interrupts (cli/sti). To avoid contention with
 *          interrupt handlers the calling program should do that. */

#include <hw/cpu/cpu.h>
#include <stdint.h>

#define SNDSB_MAX_CARDS				4

/* 0x2x0 + const = I/O port */
#define SNDSB_BIO_MIXER_INDEX			0x4
#define SNDSB_BIO_MIXER_DATA			0x5
#define SNDSB_BIO_DSP_RESET			0x6
#define SNDSB_BIO_DSP_READ_DATA			0xA
#define SNDSB_BIO_DSP_WRITE_DATA		0xC
#define SNDSB_BIO_DSP_WRITE_STATUS		0xC
#define SNDSB_BIO_DSP_READ_STATUS		0xE
#define SNDSB_BIO_DSP_READ_STATUS16		0xF

/* 0x3x0 + const = I/O port */
#define SNDSB_MPUIO_DATA			0x0
#define SNDSB_MPUIO_COMMAND			0x1
#define SNDSB_MPUIO_STATUS			0x1

/* DSP versions */
#define SNDSB_DSPCMD_GET_VERSION		0xE1

/* MPU commands */
#define SNDSB_MPUCMD_RESET			0xFF
#define SNDSB_MPUCMD_ENTER_UART			0x3F

enum {
	SNDSB_MIXER_CT1335=1,
	SNDSB_MIXER_CT1345,		/* Sound Blaster Pro chip */
	SNDSB_MIXER_CT1745,		/* Sound Blaster 16 */
	SNDSB_MIXER_MAX
};

/* which method to use when playing audio through the DSP */
enum {
	SNDSB_DSPOUTMETHOD_DIRECT=0,		/* DSP command 0x10 single-byte playback */
	SNDSB_DSPOUTMETHOD_1xx,			/* DSP command 0x14 8-bit mono (DSP 1.xx method) */
	SNDSB_DSPOUTMETHOD_200,			/* DSP command 0x1C 8-bit auto-init (DSP 2.00) */
	SNDSB_DSPOUTMETHOD_201,			/* DSP command 0x90 8-bit auto-init high speed (DSP 2.01+ using mixer bit to set stereo) */
	SNDSB_DSPOUTMETHOD_3xx,			/* DSP command 0x90 8-bit stereo auto-init (3.xx only) */
	SNDSB_DSPOUTMETHOD_4xx,			/* DSP command 0xBx/Cx 8/16-bit commands (DSP 4.xx) */
	SNDSB_DSPOUTMETHOD_MAX
};

enum {
	SNDSB_ESS_NONE=0,
	SNDSB_ESS_688,
	SNDSB_ESS_1869,
	SNDSB_ESS_MAX
};

enum {
	ADPCM_NONE=0,
	ADPCM_4BIT,
	ADPCM_2_6BIT,
	ADPCM_2BIT
};

/* NOTES: The length is the amount of data the DSP will transfer, before signalling the ISR via the SB IRQ. Usually most programs
 *        will set this to an even subdivision of the total buffer size e.g. so that a 32KB playback buffer signals IRQ every 8KB.
 *        The Sound Blaster API will take care of programming the DMA controller with the physical memory address.
 *
 *        If auto-init or DSP 4.xx commands are used, the library will try it's best but depending on the clone hardware it cannot
 *        guarantee that it can reprogram the physical memory address within the IRQ without causing one or two sample glitches,
 *        i.e. that the DSP will wait up for us when we mask DMA. In an ideal world the DSP will have a FIFO so that the time we
 *        spend DMA reprogramming is not an issue. It is said on true Creative SB16 hardware, that is precisely the case.
 *
 *        Because the SB library has control, it will automatically subdivide submitted buffers if playback would cross a 64KB
 *        boundary. But, if the physical memory address is beyond the reach of your DMA controller, the library will skip your
 *        buffer and move on. This library will provide a convenience function to check for just that.
 *
 *        Finally, remember that in 32-bit protected mode, linear addresses do not necessarily correspond to physical memory
 *        addresses. The DOS extender may be running in a DPMI environment that remaps memory. You will need to provide translation
 *        in that case. Keep in mind that when translating your linear address can be remapped in any order of physical pages, so
 *        you cannot convert just the base and assume the rest. Don't forget also that the DPMI server and environment may very well
 *        be paging memory to disk or otherwise moving it around. If you feed pages directly to this code, you must lock the pages.
 *        But if you are using DOS memory, the environment will most likely translate the DMA controller for you.
 *
 *        In most cases, the best way to handle SB playback is to use the 8237 library to allocate a fixed DMA buffer (usually in
 *        lower DOS memory) and loop through that buffer in fragments, filling in behind the play pointer.
 *
 *        the oplio variable is normally set to 0. if the Sound Blaster is Plug & Play compatible though, the OPL I/O port reported
 *        by ISA PnP will be placed there. This means that if your program wants to use the OPL3 FM synth, it should first check
 *        the oplio member and init the adlib library with that port. Else, it should assume port 0x388 and use that. Same rule
 *        applies to the gameio (gameport) member.
 *
 *        The 'aweio' variable is meant to represent the wavetable section of
 *        the Sound Blaster AWE32 and AWE64 cards. It will be set by the ISA PnP
 *        code (since those cards are ISA PnP compliant) else it will be zero. */

struct sndsb_ctx {
	uint16_t			baseio,mpuio,oplio,gameio,aweio;
	uint16_t			wssio,opl3sax_controlio;
	int8_t				dma8,dma16,irq;
	uint8_t				dsp_adpcm;
	uint8_t				audio_data_flipped_sign;	/* DSP 4.xx command allows us to specify signed/unsigned */
	uint8_t				dsp_play_method;
	uint8_t				dsp_vmaj,dsp_vmin;
	uint8_t				dsp_ok,mixer_ok;
	uint8_t				dsp_stopping;
	uint8_t				mixer_chip;
	uint8_t				dsp_record;
	uint8_t				mpu_ok;
	uint32_t			buffer_size;		/* length of the buffer IN BYTES */
	uint32_t			buffer_rate;
	uint8_t				buffer_16bit;
	uint8_t				buffer_stereo;
	uint8_t				buffer_hispeed;
	uint32_t			buffer_dma_started;	/* for 1.xx modes: the DMA offset the transfer started */
	uint32_t			buffer_dma_started_length; /* ...and for how long */
	volatile uint32_t		direct_dsp_io;		/* for direct DAC/ADC mode: software I/O pointer */
	uint32_t			buffer_irq_interval;	/* samples per IRQ. The DSP will be instructed to play this many samples, then signal the IRQ. */
								/* ADPCM modes: This becomes "bytes per IRQ" which depending on the ADPCM encoding can be 1/2, 1/3, or 1/4 the sample count. The first ADPCM block is expected to have the reference byte. */
								/* Sound blaster 1.0 and 2.0: Short intervals may cause playback to audibly pop and stutter, since DSP playback requires reprogramming per block */
	volatile uint32_t		buffer_last_io;		/* EXTERNAL USE: where the calling program last wrote/read the buffer. prepare_dma sets this to zero, but program maintains it */
	unsigned char FAR*		buffer_lin;		/* linear (program accessible pointer) */
	uint32_t			buffer_phys;
	char				dsp_copyright[128];
	/* adjustment for known weird clones of the SB chipset */
	uint8_t				is_gallant_sc6600:1;	/* Reveal/Gallant SC6600: DSP v3.5 but supports SB16 commands except for the mixer register configuration stuff */
	uint8_t				enable_adpcm_autoinit:1;/* Most emulators including DOSBox do not emulate auto-init ADPCM playback */
	uint8_t				mega_em:1;		/* Gravis MEGA-EM detected, tread very very carefully */
	uint8_t				sbos:1;			/* Gravis SBOS */
	/* options for calling library */
	uint8_t				goldplay_mode:1;	/* Goldplay mode: Set DMA transfer length to 1 (2 if stereo) and overwrite the same region of memory from timer.
								                  An early tracker/sound library released in 1991 called Goldplay does just that, which is why
										  stuff from the demoscene using that library has compatibility issued on today's hardware.
										  Sick hack, huh? */
	uint8_t				dsp_autoinit_dma:1;	/* in most cases, you can just setup the DMA buffer to auto-init loop
								   and then re-issue the playback command as normal. But some emulations,
								   especially Gravis UltraSound SBOS/MEGA-EM, don't handle that too well.
								   the only way to play properly with them is to NOT do auto-init DMA
								   and to set the DSP block size & DMA count to only precisely the IRQ
								   interval. In which case, set this to 0 */
	uint8_t				dsp_autoinit_command:1;	/* whether or not to use the auto-init form of playback/recording */
	uint8_t				dosbox_emulation:1;	/* we're running from within DOSBox */
	uint8_t				virtualbox_emulation:1;	/* we're running from within Sun/Oracle Virtualbox */
	uint8_t				windows_emulation:1;	/* we're running under Windows where the implementation is probably shitty */
	uint8_t				windows_xp_ntvdm:1;	/* Microsoft's NTVDM.EXE based emulation in Windows XP requires some restrictive
								   workarounds to buffer size, interval, etc. to work properly. Note this also
								   applies to Windows Vista. */
	uint8_t				dsp_alias_port:1;	/* if set, use DSP alias I/O port 0x22D (SB DSP 1.x and 2.x only!) EMF_ID style */
	uint8_t				backwards:1;		/* play buffer in reverse. will instruct DMA controller to decrement addr */
	uint8_t				windows_9x_me_sbemul_sys:1;/* Microsoft's SBEMUL.SYS driver, Windows 98/ME */
	uint8_t				windows_creative_sb16_drivers:1;/* Creative Sound Blaster 16 drivers for Windows */
	uint8_t				vdmsound:1;		/* We're running under VDMSOUND.EXE */
	uint8_t				do_not_probe_irq:1;	/* if the card has known configuration registers, then do not probe */
	uint8_t				do_not_probe_dma:1;	/* ... */
	uint8_t				ess_extensions:1;	/* use ESS 688/1869 extended commands */
	uint8_t				force_hispeed:1;	/* always use highspeed DSP playback commands (except for DSP 4.xx) */
	uint8_t				dsp_4xx_fifo_autoinit:1; /* SB16 use FIFO for auto-init playback */
	uint8_t				dsp_4xx_fifo_single_cycle:1; /* SB16 use FIFO for single-cycle playback */
	uint8_t				dsp_nag_mode:1;		/* "Nag" the DSP during playback (Crystal Dream style) by reissuing playback while playback is active */
	uint8_t				dsp_nag_hispeed:1;	/* "Nag" even during highspeed DMA playback (NOT recommended) */
	uint8_t				poll_ack_when_no_irq:1;	/* If playing DMA without IRQ assignment, poll the "ack" register in idle call */
	uint8_t				hispeed_matters:1;	/* If set, playing at rates above 22050Hz requires hispeed DSP commands */
	uint8_t				hispeed_blocking:1;	/* DSP does not accept commands, requires reset, in hispeed DSP playback */
	uint8_t				dsp_direct_dac_read_after_command;	/* read the DSP write status N times after direct DAC commands */
	uint8_t				dsp_direct_dac_poll_retry_timeout;	/* poll DSP write status up to N times again before attempting DSP command */
	const char*			reason_not_supported;	/* from last call can_do() or is_supported() */
/* array of mixer controls, determined by mixer chipset */
	struct sndsb_mixer_control*	sb_mixer;
	signed short			sb_mixer_items;
/* max sample rate of the DSP */
	unsigned short			max_sample_rate_dsp4xx;		/* in Hz, maximum per sample frame */
	unsigned short			max_sample_rate_sb_hispeed;	/* in Hz, maximum per sample i.e. DSP maxes out at this value at mono, or half this value at stereo */
	unsigned short			max_sample_rate_sb_hispeed_rec;	/* in Hz, maximum per sample i.e. DSP maxes out at this value at mono, or half this value at stereo */
	unsigned short			max_sample_rate_sb_play;	/* in Hz, maximum playback rate in non-hispeed mode */
	unsigned short			max_sample_rate_sb_rec;		/* in Hz, maximum recording rate in non-hispeed mode */
	unsigned short			max_sample_rate_sb_play_dac;	/* in Hz, maximum playback rate through DSP command 0x10 (Direct DAC output) */
	unsigned short			max_sample_rate_sb_rec_dac;	/* in Hz, maximum recording rate through DSP command 0x20 (Direct DAC input) */
/* function call. calling application should call this from the timer interrupt (usually IRQ 0) to
 * allow direct DAC and "goldplay" modes to work or other idle maintenance functions. if NULL, do not call. */
	void				(*timer_tick_func)(struct sndsb_ctx *cx);
/* goldplay mode DMA buffer */
#if TARGET_MSDOS == 32
	struct dma_8237_allocation*	goldplay_dma;
#else
	uint8_t				goldplay_dma[4];
#endif
	uint8_t				gold_memcpy;
/* ISA PnP information */
	const char*			pnp_name;
	uint32_t			pnp_id;
	uint8_t				pnp_csn;				/* PnP Card Select Number */
	uint8_t				pnp_bios_node;				/* PnP BIOS device node (0xFF if none) */
	uint8_t				ess_chipset;		/* which ESS chipset */
/* Windows compat hack */
	uint16_t			windows_creative_sb16_drivers_ver;
	uint8_t				windows_springwait;	/* when windows_emulation == 1, defer actually starting a DSP block until
								   the calling program has a chance to fill the buffer and check the position.
								   assume the emulation provided by Windows is the kind that (more or less)
								   directly converts DSP block commands to waveOutWrite() calls or some
								   nonsense, therefore tracking the DMA pointer and writing to the buffer
								   behind it will only cause audio glitches. Note that the test program's
								   order of calls are incompatible with Windows in this way: setting up the
								   DSP first and then setting up DMA only confuses the emulation. Doing this
								   effectively defers DSP programming until after the host program has set up
								   DMA, ensuring the minimalist emulation provided by Windows is not confused. */
	uint8_t				chose_autoinit_dma:1;	/* the library chooses to use autoinit DMA */
	uint8_t				chose_autoinit_dsp:1;	/* the library chooses to use auto-init commands */
	uint8_t				chose_use_dma:1;	/* the library chooses to run in a manner that uses DMA */
	uint8_t				direct_dac_sent_command:1;	/* direct DSP playback: we just sent the command, next is data */
	uint8_t				ess_extended_mode:1;	/* if set, ESS chip is in extended mode */
	uint8_t				timer_tick_signal:1;
};

/* runtime "options" that affect sound blaster probing.
 * note that they are only advisory flags to the library.
 * all flags are initialized to zero even prior to init_sndsb() */
struct sndsb_probe_opts {
	unsigned char			disable_manual_irq_probing:1;		/* don't use hardcore manual IRQ probing          /nomirqp */
	unsigned char			disable_alt_irq_probing:1;		/* don't use alt "lite" IRQ probing               /noairqp */
	unsigned char			disable_sb16_read_config_byte:1;	/* don't read configuration from SB16 mixer byte  /nosb16cfg */
	unsigned char			disable_manual_dma_probing:1;		/* don't probe for DMA channel if unknown         /nodmap */
	unsigned char			disable_manual_high_dma_probing:1;	/* don't probe for 16-bit DMA channel if unknown  /nohdmap */
	unsigned char			disable_windows_vxd_checks:1;		/* don't try to identify Windows drivers          /nowinvxd */
	unsigned char			disable_ess_extensions:1;		/* don't use/detect ESS688/ESS1869 commands       /noess */
	unsigned char			experimental_ess:1;			/* use ESS extensions even on chips not yet supported /ex-ess */
	unsigned char			use_dsp_alias:1;			/* probe, initialize and default to alias 22Dh    /sbalias:dsp */
};

extern struct sndsb_probe_opts		sndsb_probe_options;

#if TARGET_MSDOS == 32
extern signed char			sndsb_nmi_32_hook;
#endif

#if TARGET_MSDOS == 32 && !defined(TARGET_WINDOWS)
/* TODO: This should be moved into the hw/DOS library */
extern unsigned char			nmi_32_hooked;
extern void				(interrupt *nmi_32_old_vec)();
#endif

struct sndsb_mixer_control {
	unsigned char		index;
	unsigned char		offset:4;
	unsigned char		length:4;
	const char*		name;
};

extern const char *sndsb_dspoutmethod_str[SNDSB_DSPOUTMETHOD_MAX];
extern const char *sndsb_mixer_chip_name[SNDSB_MIXER_MAX];
extern struct sndsb_ctx sndsb_card[SNDSB_MAX_CARDS];
extern signed char gallant_sc6600_map_to_dma[4];
extern signed char gallant_sc6600_map_to_irq[8];
extern const char* sndsb_adpcm_mode_str[4];
extern struct sndsb_ctx *sndsb_card_blaster;
extern int sndsb_card_next;

struct sndsb_ctx *sndsb_by_base(uint16_t x);
struct sndsb_ctx *sndsb_by_mpu(uint16_t x);
struct sndsb_ctx *sndsb_by_irq(int8_t x);
struct sndsb_ctx *sndsb_by_dma(uint16_t x);
struct sndsb_ctx *sndsb_alloc_card();
int init_sndsb();
void free_sndsb();
void sndsb_free_card(struct sndsb_ctx *c);
struct sndsb_ctx *sndsb_try_blaster_var();
const char *sndsb_mixer_chip_str(uint8_t c);
int sndsb_read_dsp(struct sndsb_ctx *cx);
int sndsb_read_dsp_timeout(struct sndsb_ctx *cx,unsigned long timeout_ms);
int sndsb_reset_dsp(struct sndsb_ctx *cx);
int sndsb_read_mixer(struct sndsb_ctx *cx,uint8_t i);
void sndsb_write_mixer(struct sndsb_ctx *cx,uint8_t i,uint8_t d);
int sndsb_probe_mixer(struct sndsb_ctx *cx);
int sndsb_mpu_command(struct sndsb_ctx *cx,uint8_t d);
int sndsb_mpu_write(struct sndsb_ctx *cx,uint8_t d);
int sndsb_mpu_read(struct sndsb_ctx *cx);
int sndsb_probe_mpu401(struct sndsb_ctx *cx);
int sndsb_write_dsp(struct sndsb_ctx *cx,uint8_t d);
int sndsb_write_dsp_timeout(struct sndsb_ctx *cx,uint8_t d,unsigned long timeout_ms);
int sndsb_write_dsp_timeconst(struct sndsb_ctx *cx,uint8_t tc);
int sndsb_query_dsp_version(struct sndsb_ctx *cx);
int sndsb_init_card(struct sndsb_ctx *cx);
int sndsb_try_base(uint16_t iobase);
int sndsb_interrupt_reason(struct sndsb_ctx *cx);
int sndsb_reset_mixer(struct sndsb_ctx *cx);
int sndsb_dsp_out_method_supported(struct sndsb_ctx *cx,unsigned long wav_sample_rate,unsigned char wav_stereo,unsigned char wav_16bit);
int sndsb_write_dsp_blocksize(struct sndsb_ctx *cx,uint16_t tc);
int sndsb_write_dsp_outrate(struct sndsb_ctx *cx,unsigned long rate);
uint32_t sndsb_read_dma_buffer_position(struct sndsb_ctx *cx);
int sndsb_shutdown_dma(struct sndsb_ctx *cx);
int sndsb_setup_dma(struct sndsb_ctx *cx);
int sndsb_prepare_dsp_playback(struct sndsb_ctx *cx,unsigned long rate,unsigned char stereo,unsigned char bit16);
int sndsb_begin_dsp_playback(struct sndsb_ctx *cx);
int sndsb_stop_dsp_playback(struct sndsb_ctx *cx);
void sndsb_send_buffer_again(struct sndsb_ctx *cx);
int sndsb_determine_ideal_dsp_play_method(struct sndsb_ctx *cx);
void sndsb_choose_mixer(struct sndsb_ctx *card,signed char override);
int sndsb_submit_buffer(struct sndsb_ctx *cx,unsigned char FAR *ptr,uint32_t phys,uint32_t len,uint32_t user,uint8_t loop);
int sndsb_assign_dma_buffer(struct sndsb_ctx *cx,struct dma_8237_allocation *dma);
unsigned char sndsb_rate_to_time_constant(struct sndsb_ctx *cx,unsigned long rate);
unsigned int sndsb_ess_set_extended_mode(struct sndsb_ctx *cx,int enable);
int sndsb_ess_read_controller(struct sndsb_ctx *cx,int reg);
int sndsb_ess_write_controller(struct sndsb_ctx *cx,int reg,unsigned char value);
void sndsb_irq_continue(struct sndsb_ctx *cx,unsigned char c);
unsigned int sndsb_will_dsp_nag(struct sndsb_ctx *cx);

int sb_nmi_32_auto_choose_hook();
#if TARGET_MSDOS == 32
void do_nmi_32_unhook();
void do_nmi_32_hook();
#else
# define do_nmi_32_unhook()
# define do_nmi_32_hook()
#endif

static inline unsigned char sndsb_ctx_to_index(struct sndsb_ctx *c) {
	return (unsigned char)(((size_t)c - (size_t)sndsb_card) / sizeof(struct sndsb_ctx));
}

static inline struct sndsb_ctx *sndsb_index_to_ctx(unsigned char c) {
	return sndsb_card + c;
}

static inline int sndsb_recommend_vm_wait(struct sndsb_ctx *cx) {
	/* Microsoft Windows XP and NTVDM.EXE Sound Blaster emulation:
	 *   - Emulation is terrible. Audio can skip and stutter slightly.
	 *     Giving up your timeslice only guarantees it will stutter a lot *WORSE* */
	if (cx->windows_emulation && cx->windows_xp_ntvdm) return 0;

	/* Microsoft Windows XP and VDMSOUND. Yielding is recommended,
	 * but you will get very stuttery sound with small block sizes! */
	if (cx->windows_emulation && cx->buffer_irq_interval < (cx->buffer_rate/4)) return 0;

	/* otherwise, yes. go ahead */
	return 1;
}

static inline void sndsb_interrupt_ack(struct sndsb_ctx *cx,unsigned char c) {
	if (c & 1) inp(cx->baseio + SNDSB_BIO_DSP_READ_STATUS);
	if (c & 2) inp(cx->baseio + SNDSB_BIO_DSP_READ_STATUS16);
}

static inline void sndsb_dsp_direct_output(struct sndsb_ctx *cx,unsigned char c) {
	sndsb_write_dsp(cx,0x10);
	sndsb_write_dsp(cx,c);
}

void sndsb_main_idle(struct sndsb_ctx *cx);

/* Sound Blaster ADPCM encoding routines */
#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
#else
unsigned char sndsb_encode_adpcm_4bit(unsigned char samp);
unsigned char sndsb_encode_adpcm_2bit(unsigned char samp);
unsigned char sndsb_encode_adpcm_2_6bit(unsigned char samp,unsigned char b2);
void sndsb_encode_adpcm_set_reference(unsigned char c,unsigned char mode);
void sndsb_encode_adpcm_reset_wo_ref(unsigned char mode);
void sndsb_alt_lite_probe_irq(struct sndsb_ctx *cx);
void sndsb_manual_probe_irq(struct sndsb_ctx *cx);
void sndsb_manual_probe_dma(struct sndsb_ctx *cx);
#endif

void sndsb_write_mixer_entry(struct sndsb_ctx *sb,struct sndsb_mixer_control *mc,unsigned char nb);
unsigned char sndsb_read_mixer_entry(struct sndsb_ctx *sb,struct sndsb_mixer_control *mc);
unsigned long sndsb_real_sample_rate(struct sndsb_ctx *cx);

uint32_t sndsb_recommended_dma_buffer_size(struct sndsb_ctx *ctx,uint32_t limit);

void sndsb_timer_tick_directi_data(struct sndsb_ctx *cx);
void sndsb_timer_tick_directi_cmd(struct sndsb_ctx *cx);
void sndsb_timer_tick_directo_data(struct sndsb_ctx *cx);
void sndsb_timer_tick_directo_cmd(struct sndsb_ctx *cx);

const char *sndsb_ess_chipset_str(unsigned int c);

#if TARGET_MSDOS == 32
int sb_nmi_32_auto_choose_hook();
#endif

