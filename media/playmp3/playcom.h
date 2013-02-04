
#include <ext/libmad/mad.h>

#include <ext/faad/aacaudio.h>
#include <ext/faad/neaacdec.h>

#include <ext/libogg/ogg.h>

#include <ext/vorbis/codec.h>

#include <ext/speex/speex.h>
#include <ext/speex/speex_bits.h>

#include "qtreader.h"

#define mp3_data_size (16*1024)

#define COMMON_ABOUT_HELP_STR	"WAV/MP3/AAC/OGG/MP4/MOV audio player v1.0 for DOS\n(C) 2008-2012 Jonathan Campbell ALL RIGHTS RESERVED\n" \
	"32-bit protected mode version\n" \
	"\n" \
	"Uses libmad MP3 decoder library (C) 2000-2004 Underbit Technologies, Inc.\n" \
	"Uses faad2 AAC decoder library (C) 2003-2005 M. Bakker, Nero AG\n" \
	"Uses libogg, Vorbis, Speex and FLAC libraries (C) Xiph foundation"

enum {
	TYPE_MP3=0,
	TYPE_AAC,
	TYPE_WAV,
	TYPE_OGG,	/* Ogg Vorbis, Ogg Speex, ... */
	TYPE_MP4,
	TYPE_NULL
};

/* basic WAVEFORMAT structure.
 * for simplicity purposes we do not bother with the WAVEFORMATEXTENSIBLE or
 * any of the compressed formats.... yet */
#pragma pack(push,1)
typedef struct {
	uint16_t  wFormatTag;
	uint16_t  nChannels;
	uint32_t  nSamplesPerSec;
	uint32_t  nAvgBytesPerSec;
	uint16_t  nBlockAlign;
	uint16_t  wBitsPerSample;
} windows_WAVEFORMAT;
#pragma pack(pop)

extern char				temp_str[512];
extern unsigned char			animator;
extern int				mp3_file_type;
extern int				mp3_fd;
extern char				mp3_file[130];
extern unsigned char			mp3_stereo,mp3_16bit,mp3_bytes_per_sample;
extern unsigned long			mp3_data_offset,mp3_data_length,mp3_sample_rate;
extern unsigned long			mp3_sample_rate_by_timer_ticks;
extern unsigned long			mp3_sample_rate_by_timer;
extern unsigned long			mp3_smallstep;
extern unsigned char			mp3_flipsign;
extern unsigned char			mp3_playing;

extern unsigned char			wav_stereo,wav_16bit;

extern struct mpeg_aac_adts_audio_sync_info aac_info,aac_ninfo;
extern mpeg_aac_adts_audio_sync_raw_pattern aac_sync,aac_nsync;

/* faad decoding object */
extern NeAACDecHandle			aac_dec_handle;
extern unsigned char			aac_dec_init;

extern quicktime_reader*		qt_reader;

/* faad decoded audio */
extern int16_t				*aac_dec_last_audio,*aac_dec_last_audio_fence;
extern NeAACDecFrameInfo		aac_dec_last_info;

extern ogg_sync_state*			ogg_sync;
extern ogg_page*			ogg_sync_page;
extern ogg_stream_state*		ogg_sync_stream;

/* libmad decoding state */
extern unsigned char			mad_init;
extern struct mad_stream		mad_stream;
extern struct mad_frame			mad_frame;
extern struct mad_synth			mad_synth;
extern unsigned char			mad_synth_ready;
extern unsigned int			mad_synth_readofs;
extern unsigned char			mp3_data[mp3_data_size],*mp3_data_read,*mp3_data_write;
extern volatile unsigned char		sb_irq_count;
extern volatile unsigned char		IRQ_anim;
extern uint32_t				irq_0_count;
extern uint32_t				irq_0_adv;
extern uint32_t				irq_0_max;
extern uint8_t				irq_0_sent_command;
extern unsigned char			irq_0_had_warned;
extern volatile uint32_t		irq_0_watchdog;
extern uint32_t				last_dma_position;
extern int				change_param_idx;
#define PARAM_PRESET_RATES 17
extern const unsigned short		param_preset_rates[PARAM_PRESET_RATES];
extern const char*			dos32_irq_0_warning;

#if !defined(TARGET_WINDOWS)
extern const struct vga_menu_item menu_separator;
extern const struct vga_menu_item main_menu_file_set;
extern const struct vga_menu_item main_menu_file_quit;
extern const struct vga_menu_item main_menu_playback_play;
extern const struct vga_menu_item main_menu_playback_stop;
extern const struct vga_menu_item main_menu_playback_params;
extern const struct vga_menu_item main_menu_help_about;
#endif

void mp3_data_clear();
void mp3_data_refill();
void mp3_data_flush(unsigned char force);

void free_libmad();
unsigned char init_libmad();

void free_faad2();
unsigned char init_faad2();

void draw_irq_indicator();
void mad_reset_decoder();
void irq_0_watchdog_do();
void irq_0_watchdog_ack();
void irq_0_watchdog_reset();
void vga_write_until(unsigned int x);
void prompt_play_mp3(unsigned char rec);
int confirm_quit();
void close_mp3();
void open_mp3();
int mad_more();

extern void (interrupt *old_irq_0)();

static inline unsigned char xdigit2int(char c) {
	if (c >= '0' && c <= '9')
		return (unsigned char)(c - '0');
	else if (c >= 'a' && c <= 'f')
		return (unsigned char)(c - 'a' + 10);
	else if (c >= 'A' && c <= 'F')
		return (unsigned char)(c - 'A' + 10);
	return 0;
}

/* provided by the calling program */
void ui_anim(int force);
void begin_play();
void stop_play();

