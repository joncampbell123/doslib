
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <assert.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#if defined(TARGET_WINDOWS)
# include <windows.h>
# include <windows/apihelp.h>
# include "resource.h"

extern HWND		hwndMain;
extern HINSTANCE	myInstance;
extern HMENU		FileMenu;
extern HMENU		AppMenu;
extern HICON		AppIcon;
#else
# include <hw/vga/vga.h>
# include <hw/dos/dos.h>
# include <hw/8237/8237.h>		/* 8237 DMA */
# include <hw/8254/8254.h>		/* 8254 timer */
# include <hw/8259/8259.h>		/* 8259 PIC interrupts */
# include <hw/vga/vgagui.h>
# include <hw/vga/vgatty.h>
#endif

#include "playcom.h"

char				temp_str[512];
unsigned char			animator = 0;

int				mp3_file_type = TYPE_MP3;

int				mp3_fd = -1;
char				mp3_file[130] = {0};
unsigned char			mp3_stereo = 0,mp3_16bit = 0,mp3_bytes_per_sample = 1;
unsigned long			mp3_data_offset = 44,mp3_data_length = 0,mp3_sample_rate = 8000;
unsigned long			mp3_smallstep = 128*1024;
unsigned char			mp3_playing = 0;

/* libmad decoding state */
unsigned char			mad_init=0;
struct mad_stream		mad_stream;
struct mad_frame		mad_frame;
struct mad_synth		mad_synth;
unsigned char			mad_synth_ready=0;
unsigned int			mad_synth_readofs=0;
unsigned char			mp3_data[mp3_data_size],*mp3_data_read=NULL,*mp3_data_write=NULL;

unsigned char			wav_stereo=0,wav_16bit=0;

quicktime_reader*		qt_reader = NULL;
int				qt_audio_index = -1;
unsigned long			qt_audio_chunk = -1,qt_audio_sample = -1;
int				qt_audio_codec = -1;

struct mpeg_aac_adts_audio_sync_info aac_info={0},aac_ninfo={0};
mpeg_aac_adts_audio_sync_raw_pattern aac_sync=0,aac_nsync=0;

/* faad decoding object */
NeAACDecHandle			aac_dec_handle = NULL;
unsigned char			aac_dec_init = 0;

/* faad decoded audio */
int16_t				*aac_dec_last_audio = NULL,*aac_dec_last_audio_fence = NULL;
NeAACDecFrameInfo		aac_dec_last_info;
unsigned char*			aac_esds = NULL;
size_t				aac_esds_size = 0;

/* ogg bitstream */
ogg_sync_state*			ogg_sync = NULL;
ogg_page*			ogg_sync_page = NULL;
ogg_stream_state*		ogg_sync_stream = NULL;
int				ogg_read_no_loop = 0;
int				ogg_codec = 0;

/* vorbis codec */
vorbis_info*			ogg_vorbis_info = NULL;
vorbis_comment*			ogg_vorbis_comment = NULL;
vorbis_dsp_state*		ogg_vorbis_dsp_state = NULL;
vorbis_block*			ogg_vorbis_block = NULL;

/* speex codec */
const SpeexMode*		ogg_speex_mode = NULL;
void*				ogg_speex_state = NULL;
short*				ogg_speex_out = NULL;
size_t				ogg_speex_frame_size = 0;
int				ogg_speex_rate = 8000;
int				ogg_speex_channels = 1;

enum {
	OGG_NONE=0,
	OGG_VORBIS,
	OGG_SPEEX,
	OGG_FLAC
};

volatile unsigned char		IRQ_anim = 0;
volatile unsigned char		sb_irq_count = 0;

uint32_t			irq_0_count = 0;
uint32_t			irq_0_adv = 1;
uint32_t			irq_0_max = 1;
uint8_t				irq_0_sent_command = 0;
unsigned char			irq_0_had_warned = 0;

/* IRQ 0 watchdog: when playing audio it is possible to exceed the rate
   that the CPU can possibly handle servicing the interrupt. This results
   in audio that still plays but the UI is "frozen" because no CPU time
   is available. If the UI is not there to reset the watchdog, the ISR
   will auto-stop playback, allowing the user to regain control without
   hitting the RESET button. */
volatile uint32_t		irq_0_watchdog = 0x10000UL;
uint32_t			last_dma_position = 1;
int				change_param_idx = 0;

void				(interrupt *old_irq_0)() = NULL;

const char *dos32_irq_0_warning =
	"WARNING: The timer is made to run at the sample rate. Depending on your\n"
	"         DOS extender there may be enough overhead to overwhelm the CPU\n"
	"         and possibly cause a crash.\n"
	"         Enable?";

#if !defined(TARGET_WINDOWS)
const struct vga_menu_item menu_separator =
	{(char*)1,		0,	0,	0};
const struct vga_menu_item main_menu_file_set =
	{"Set file...",		's',	0,	0};
const struct vga_menu_item main_menu_file_quit =
	{"Quit",		'q',	0,	0};
const struct vga_menu_item main_menu_playback_play =
	{"Play",		'p',	0,	0};
const struct vga_menu_item main_menu_playback_stop =
	{"Stop",		's',	0,	0};
const struct vga_menu_item main_menu_playback_params =
	{"Parameters",		'a',	0,	0};
const struct vga_menu_item main_menu_help_about =
	{"About",		'r',	0,	0};
#endif

void mp3_data_clear() {
	mp3_data_read = mp3_data_write = mp3_data;
	mad_synth_readofs = 0;
	mad_synth_ready = 0;
}

int mad_more_null();

void mp3_data_refill() {
	int rd;

	if (mp3_data_write < mp3_data+mp3_data_size) {
		size_t w = (size_t)(mp3_data+mp3_data_size-mp3_data_write);
		rd = read(mp3_fd,mp3_data_write,w);
		if (rd < (int)w) {
			if (rd > 0) mp3_data_write += rd;
			lseek(mp3_fd,mp3_data_offset,SEEK_SET);
			w = (size_t)(mp3_data+mp3_data_size-mp3_data_write);
			rd = read(mp3_fd,mp3_data_write,w);
		}
		if (rd > 0) mp3_data_write += rd;
	}
}

void mp3_data_flush(unsigned char force) {
	assert(mp3_data_read != NULL);
	assert(mp3_data_write != NULL);
	assert(mp3_data_read >= mp3_data && mp3_data_read <= (mp3_data+mp3_data_size));
	assert(mp3_data_write >= mp3_data && mp3_data_write <= (mp3_data+mp3_data_size));
	assert(mp3_data_read <= mp3_data_write);

	if (force || mp3_data_read >= (mp3_data+(mp3_data_size/2))) {
		size_t l = (size_t)(mp3_data_write - mp3_data_read);
		if (l != 0) memmove(mp3_data,mp3_data_read,l);
		mp3_data_read = mp3_data;
		mp3_data_write = mp3_data + l;
	}
}

unsigned char init_libogg() {
	if (ogg_sync == NULL) {
		ogg_sync = (ogg_sync_state*)malloc(sizeof(ogg_sync_state));
		if (ogg_sync != NULL) ogg_sync_init(ogg_sync);
	}
	if (ogg_sync_page == NULL) {
		ogg_sync_page = (ogg_page*)malloc(sizeof(ogg_page));
		if (ogg_sync_page != NULL) memset(ogg_sync_page,0,sizeof(ogg_page));
	}

	return (ogg_sync != NULL) && (ogg_sync_page != NULL);
}

void free_speex() {
	if (ogg_speex_state) {
		speex_decoder_destroy(ogg_speex_state);
		ogg_speex_state = NULL;
	}
	if (ogg_speex_out) {
		free(ogg_speex_out);
		ogg_speex_out = NULL;
	}
	ogg_speex_mode = NULL;
	ogg_speex_frame_size = 0;
}

void free_vorbis() {
	if (ogg_vorbis_block) {
		vorbis_block_clear(ogg_vorbis_block);
		free(ogg_vorbis_block);
		ogg_vorbis_block = NULL;
	}
	if (ogg_vorbis_dsp_state) {
		vorbis_dsp_clear(ogg_vorbis_dsp_state);
		free(ogg_vorbis_dsp_state);
		ogg_vorbis_dsp_state = NULL;
	}
	if (ogg_vorbis_comment) {
		vorbis_comment_clear(ogg_vorbis_comment);
		free(ogg_vorbis_comment);
		ogg_vorbis_comment = NULL;
	}
	if (ogg_vorbis_info) {
		vorbis_info_clear(ogg_vorbis_info);
		free(ogg_vorbis_info);
		ogg_vorbis_info = NULL;
	}
}

void free_libogg() {
	if (ogg_sync_stream) {
		ogg_stream_clear(ogg_sync_stream);
		free(ogg_sync_stream);
		ogg_sync_stream = NULL;
	}
	if (ogg_sync_page) {
		free(ogg_sync_page);
		ogg_sync_page = NULL;
	}
	if (ogg_sync) {
		ogg_sync_clear(ogg_sync);
		free(ogg_sync);
		ogg_sync = NULL;
	}
}

void free_libmad() {
	if (mad_init) {
		mad_stream_finish(&mad_stream);
		mad_frame_finish(&mad_frame);
		memset(&mad_frame,0,sizeof(mad_frame));

		mad_synth_finish(&mad_synth);
		memset(&mad_synth,0,sizeof(mad_synth));

		memset(&mad_stream,0,sizeof(mad_stream));
		mad_init = 0;
	}
}

unsigned char init_libmad() {
	if (!mad_init) {
		mad_frame_init(&mad_frame);
		mad_synth_init(&mad_synth);
		mad_stream_init(&mad_stream);
		mad_init = 1;
	}

	return mad_init;
}

void free_faad2() {
	aac_dec_last_audio = aac_dec_last_audio_fence = NULL;
	aac_dec_init = 0;
	if (aac_dec_handle != NULL) {
		NeAACDecClose(aac_dec_handle);
		aac_dec_handle = NULL;
	}
}

unsigned char init_faad2() {
	if (aac_dec_handle == NULL) {
		aac_dec_init = 0;
		aac_dec_last_audio = aac_dec_last_audio_fence = NULL;
		if ((aac_dec_handle = NeAACDecOpen()) == NULL)
			return 0;
	}

	return (aac_dec_handle != NULL)?1:0;
}

void irq_0_watchdog_do() {
	if (irq_0_watchdog > 0UL) {
		if (--irq_0_watchdog == 0UL) {
			/* try to help by setting the timer rate back down */
#if !defined(TARGET_WINDOWS)
			write_8254_system_timer(0); /* restore 18.2 tick/sec */
#endif
			irq_0_count = 0;
			irq_0_adv = 1;
			irq_0_max = 1;
		}
	}
}

void irq_0_watchdog_ack() {
	if (irq_0_watchdog != 0UL) {
		irq_0_watchdog += 0x4000UL; /* 1/4 of max. This should trigger even if UI is only reduced to tar speeds by ISR */
		if (irq_0_watchdog > 0x10000UL) irq_0_watchdog = 0x10000UL;
	}
}

void irq_0_watchdog_reset() {
	irq_0_watchdog = 0x10000UL;
}

int mad_qt_aac_decode() {
	if (!aac_dec_init) {
		unsigned long rate;
		unsigned char chan;

		if (aac_esds == NULL || aac_esds_size == 0) {
			mp3_data_clear();
			mp3_file_type = TYPE_NULL;
			return mad_more_null();
		}

		if ((long)NeAACDecInit2(aac_dec_handle,aac_esds,aac_esds_size,&rate,&chan) >= 0L) {
			aac_dec_init = 1;
		}
		else {
			free_faad2();
			init_faad2();
			return 0;
		}
	}

	if (aac_dec_init) {
		aac_dec_last_audio = (int16_t*)NeAACDecDecode(aac_dec_handle,&aac_dec_last_info,
			mp3_data_read,(size_t)(mp3_data_write - mp3_data_read));
		if (aac_dec_last_audio != NULL) {
			aac_dec_last_audio_fence = aac_dec_last_audio + aac_dec_last_info.samples;
		}
		else {
			free_faad2();
			init_faad2();
			return 0;
		}
	}
	else {
		free_faad2();
		init_faad2();
		return 0;
	}

	return 1;
}

int mad_more_qt() {
	int rd;

	if (qt_audio_index >= 0) {
		if (qt_audio_codec == TYPE_AAC) {
			if (aac_dec_last_audio != NULL && aac_dec_last_audio >= aac_dec_last_audio_fence)
				aac_dec_last_audio = aac_dec_last_audio_fence = NULL;

			if (aac_dec_last_audio == NULL) {
				quicktime_reader_track *trk = qt_reader->track + qt_audio_index;
				quicktime_reader_stsc_entry *ent;
				size_t len,sample;

				if ((long)qt_audio_chunk < 0L) {
					qt_audio_chunk = 0;
					qt_audio_sample = 0;
				}
				if (qt_audio_chunk >= trk->chunk.total) {
					qt_audio_chunk = 0;
					qt_audio_sample = 0;
				}
				if (qt_audio_chunk >= trk->chunk.total)
					return 0;

				ent = trk->sampletochunk.list + qt_audio_chunk;
				if (qt_audio_sample >= ent->samples_per_chunk) {
					qt_audio_chunk++;
					qt_audio_sample = 0;
					return 0;
				}

				if (qt_audio_sample == 0)
					lseek(mp3_fd,trk->chunk.list[qt_audio_chunk],SEEK_SET);

				sample = ent->first_sample + qt_audio_sample;
				if (trk->samplesize.list == NULL || sample >= trk->samplesize.entries) {
					qt_audio_chunk = qt_audio_sample = 0;
					return 0;
				}
				len = trk->samplesize.list[sample];

				mp3_data_clear(0);
				if (len <= mp3_data_size) {
					mp3_data_read = mp3_data;
					rd = read(mp3_fd,mp3_data,(int)len);
					if (rd < 0) rd = 0;
					assert(rd <= mp3_data_size);
					mp3_data_write = mp3_data + rd;
					mad_qt_aac_decode();
				}

				if ((++qt_audio_sample) >= ent->samples_per_chunk) {
					qt_audio_chunk++;
					qt_audio_sample = 0;
				}
			}
		}
		else {
			aac_dec_last_audio = aac_dec_last_audio_fence = NULL;
		}
	}
	else {
		aac_dec_last_audio = aac_dec_last_audio_fence = NULL;
	}

	return 1;
}

int mad_more_aac() {
	if (aac_dec_last_audio != NULL && aac_dec_last_audio >= aac_dec_last_audio_fence)
		aac_dec_last_audio = aac_dec_last_audio_fence = NULL;

	if (aac_dec_last_audio == NULL) {
		mp3_data_flush(0);
		mp3_data_refill();

		mp3_data_read = mpeg_aac_adts_audio_sync(mp3_data_read,mp3_data_write,&aac_nsync,&aac_ninfo);
		if (aac_nsync != 0ULL && aac_ninfo.sample_rate != 0) {
			if ((mp3_data_read+aac_ninfo.aac_frame_length+7) > mp3_data_write) {
				mp3_data_flush(1);
				mp3_data_refill();
				return 0;
			}
			else {
				if (!aac_dec_init) {
					unsigned long rate;
					unsigned char chan;
					mp3_data_flush(1);
					mp3_data_refill();
					if (NeAACDecInit(aac_dec_handle,mp3_data_read,(size_t)(mp3_data_write - mp3_data_read),&rate,&chan) >= 0) {
						aac_dec_init = 1;
					}
					else {
						free_faad2();
						init_faad2();
						mp3_data_read++;
						return 0;
					}
				}

				if (aac_dec_init) {
					aac_dec_last_audio = (int16_t*)NeAACDecDecode(aac_dec_handle,&aac_dec_last_info,
						mp3_data_read,aac_ninfo.aac_frame_length);
					if (aac_dec_last_audio != NULL) {
						aac_dec_last_audio_fence = aac_dec_last_audio + aac_dec_last_info.samples;
					}
					else {
						free_faad2();
						init_faad2();
						mp3_data_read++;
						return 0;
					}

					if (aac_dec_last_info.bytesconsumed != 0)
						mp3_data_read += aac_dec_last_info.bytesconsumed;
					else
						mp3_data_read += aac_ninfo.aac_frame_length;
				}
				else {
					free_faad2();
					init_faad2();
					mp3_data_read++;
					return 0;
				}
			}
		}
		else {
			free_faad2();
			init_faad2();
			mp3_data_read++;
			return 0;
		}
	}

	return 1;
}

int mad_more_mp3() {
	if (mad_synth_ready && mad_synth_readofs >= mad_synth.pcm.length)
		mad_synth_ready = 0;

	if (!mad_synth_ready) {
		mp3_data_flush(0);
		mp3_data_refill();

		mad_stream_buffer(&mad_stream,mp3_data_read,(size_t)(mp3_data_write - mp3_data_read));
		mad_stream.sync = 0;
		mad_stream.skiplen = 0;
		mad_header_decode(&mad_frame.header,&mad_stream);

		if (!mad_stream.sync) {
			if (mp3_data_read == mp3_data) {
				mp3_data_clear();
				mp3_data_refill();
			}
			else {
				mp3_data_flush(1);
				mp3_data_refill();
			}

			return 0;
		}

		if (mad_frame_decode(&mad_frame,&mad_stream) == -1) {
			if (mp3_data_read == mp3_data) {
				mp3_data_read += 32;
			}
			else {
				mp3_data_flush(1);
				mp3_data_refill();
			}

			return 0;
		}

		mad_synth_frame(&mad_synth,&mad_frame);
		if (mad_synth.pcm.length == 0)
			return 0;

		mad_synth_ready = 1;
		mad_synth_readofs = 0;

		if (mad_stream.next_frame == NULL)
			mp3_data_read += 4;
		else
			mp3_data_read = (unsigned char*)mad_stream.next_frame;
		assert(mp3_data_read >= mp3_data && mp3_data_read <= mp3_data+mp3_data_size);
	}

	return 1;
}

int mad_more_wav() {
	if (mp3_data_read == mp3_data_write) {
		unsigned long pos = lseek(mp3_fd,0,SEEK_CUR);
		unsigned long rem;
		int rd,i;

		if (pos < mp3_data_offset)
			lseek(mp3_fd,pos=mp3_data_offset,SEEK_SET);
		else if (pos >= (mp3_data_offset+mp3_data_length))
			lseek(mp3_fd,pos=mp3_data_offset,SEEK_SET);

		pos -= mp3_data_offset;
		pos -= pos % mp3_bytes_per_sample;
		pos += mp3_data_offset;

		rem = (mp3_data_offset+mp3_data_length) - pos;
		if (rem > (unsigned long)sizeof(mp3_data))
			rem = (unsigned long)sizeof(mp3_data);

		if (!wav_16bit && mp3_16bit)
			rem >>= 1UL;

		mp3_data_read = mp3_data;
		rd = read(mp3_fd,mp3_data,(unsigned int)rem);
		if (rd < (unsigned int)rem) {
			lseek(mp3_fd,mp3_data_offset,SEEK_SET);
			if (rd <= 0) return 0;
		}

		if (wav_16bit && !mp3_16bit) { /* convert in-place 16-bit signed -> 8-bit unsigned */
			rd >>= 1;
			for (i=0;i < rd;i++)
				mp3_data[i] = (unsigned char)((((int16_t*)mp3_data)[i] >> 8) + 0x80);
		}
		else if (!wav_16bit && mp3_16bit) { /* convert in-place 8-bit unsigned -> 16-bit signed */
			for (i=rd-1;i >= 0;i--)
				((int16_t*)mp3_data)[i] = ((int16_t)((uint8_t*)mp3_data)[i] << 8U) - 0x8000;

			rd <<= 1;
		}

		assert(rd <= sizeof(mp3_data));
		mp3_data_write = mp3_data + rd;
	}

	return 1;
}

int mad_more_null() {
	if (mp3_data_read == mp3_data_write) {
		/* we "send" data to the main loop directly using the mp3_data buffer */
		memset(mp3_data,0,1024 * mp3_bytes_per_sample);
		mp3_data_read = mp3_data;
		mp3_data_write = mp3_data + (1024 * mp3_bytes_per_sample);
	}

	return 1;
}

int next_ogg_page() {
	int rd,patience=100;

	if (ogg_sync == NULL)
		return 0;

	rd = ogg_sync_pageout(ogg_sync,ogg_sync_page);
	while (rd != 1) {
		unsigned char *ptr = ogg_sync_buffer(ogg_sync,32768);
		if (ptr == NULL) return 0;
		if (--patience <= 0) return 0;
		rd = read(mp3_fd,ptr,32768);
		if (rd <= 0) {
			if (ogg_read_no_loop) return 0;
			lseek(mp3_fd,0,SEEK_SET);
			rd = read(mp3_fd,ptr,32768);
		}
		if (rd < 0) rd = 0;
		ogg_sync_wrote(ogg_sync,rd);
		rd = ogg_sync_pageout(ogg_sync,ogg_sync_page);
	}

	return (rd == 1);
}

int next_ogg_packet(ogg_packet *pkt) {
	int patience;

	if (ogg_sync == NULL || ogg_sync_stream == NULL)
		return 0;

	patience = 100;
	while (ogg_stream_packetout(ogg_sync_stream,pkt) != 1) {
		do {
			while (!next_ogg_page()) {
				if (--patience <= 0) return 0;
			}
		} while (ogg_stream_pagein(ogg_sync_stream,ogg_sync_page) != 0);
	}

	return 1;
}

int ogg_known_codec(ogg_packet *pkt) {
	if (!memcmp(pkt->packet,"\x01" "vorbis",7))
		return OGG_VORBIS;
	if (!memcmp(pkt->packet,"Speex",5))
		return OGG_SPEEX;

	return 0;
}

int init_speex(ogg_packet *pkt) {
	unsigned int rate;
	unsigned int ch;
	int frsz,i;

	if (pkt == NULL) return 0;

	rate = *((uint32_t*)(pkt->packet+36));
	ch = *((uint32_t*)(pkt->packet+48));

	if (rate >= 32000)
		ogg_speex_mode = &speex_uwb_mode;
	else if (rate >= 16000)
		ogg_speex_mode = &speex_wb_mode;
	else
		ogg_speex_mode = &speex_nb_mode;

	ogg_speex_state = speex_decoder_init(ogg_speex_mode);
	if (ogg_speex_state == NULL) {
		fprintf(stderr,"Cannot init Speex decoder\n");
		free_speex();
		return 0;
	}

	frsz = 159;
	speex_decoder_ctl(ogg_speex_state,SPEEX_GET_FRAME_SIZE,&frsz);
	ogg_speex_frame_size = frsz;
	ogg_speex_rate = rate;
	ogg_speex_channels = ch;
	ogg_speex_out = (short*)malloc(sizeof(short) * ogg_speex_frame_size * ogg_speex_channels);
	if (ogg_speex_out == NULL) {
		fprintf(stderr,"Cannot alloc speex out\n");
		free_speex();
		return 0;
	}
	memset(ogg_speex_out,0,sizeof(short) * ogg_speex_frame_size * ogg_speex_channels);

	/* disable the "enhancement" it only serves to screw with the volume level */
	i = 0;
	speex_decoder_ctl(ogg_speex_state,SPEEX_SET_ENH,&i);

	/* skip the next one */
	if (!next_ogg_packet(pkt)) {
		fprintf(stderr,"Unable to get packet #2\n");
		free_vorbis();
		return 0;
	}

	wav_stereo = (ogg_vorbis_info->channels >= 2);
	wav_16bit = 1;
	return 1;
}

int init_vorbis(ogg_packet *pkt) {
	if (pkt == NULL) return 0;

	if (ogg_vorbis_info == NULL) {
		ogg_vorbis_info = malloc(sizeof(*ogg_vorbis_info));
		if (ogg_vorbis_info == NULL) return 0;
		memset(ogg_vorbis_info,0,sizeof(*ogg_vorbis_info));
		vorbis_info_init(ogg_vorbis_info);
	}

	if (ogg_vorbis_comment == NULL) {
		ogg_vorbis_comment = malloc(sizeof(*ogg_vorbis_comment));
		if (ogg_vorbis_comment == NULL) return 0;
		memset(ogg_vorbis_comment,0,sizeof(*ogg_vorbis_comment));
		vorbis_comment_init(ogg_vorbis_comment);
	}

	/* now feed the packet to vorbis */
	if (vorbis_synthesis_headerin(ogg_vorbis_info,ogg_vorbis_comment,pkt) != 0) {
		fprintf(stderr,"Vorbis codec rejected first packet\n");
		free_vorbis();
		return 0;
	}

	/* feed the next two header packets to vorbis */
	if (!next_ogg_packet(pkt)) {
		fprintf(stderr,"Unable to get packet #2\n");
		free_vorbis();
		return 0;
	}

	if (vorbis_synthesis_headerin(ogg_vorbis_info,ogg_vorbis_comment,pkt) != 0) {
		fprintf(stderr,"Unable to apply packet #2\n");
		free_vorbis();
		return 0;
	}

	/* feed the next two header packets to vorbis */
	if (!next_ogg_packet(pkt)) {
		fprintf(stderr,"Unable to get packet #3\n");
		free_vorbis();
		return 0;
	}

	if (vorbis_synthesis_headerin(ogg_vorbis_info,ogg_vorbis_comment,pkt) != 0) {
		fprintf(stderr,"Unable to apply packet #3\n");
		free_vorbis();
		return 0;
	}

	/* init the DSP state */
	ogg_vorbis_dsp_state = malloc(sizeof(*ogg_vorbis_dsp_state));
	if (ogg_vorbis_dsp_state == NULL) {
		fprintf(stderr,"Cannot alloc DSP state\n");
		free_vorbis();
		return 0;
	}

	if (vorbis_synthesis_init(ogg_vorbis_dsp_state,ogg_vorbis_info) != 0) {
		fprintf(stderr,"Vorbis synthesis init failed\n");
		free_vorbis();
		return 0;
	}

	ogg_vorbis_block = malloc(sizeof(*ogg_vorbis_block));
	if (ogg_vorbis_block == NULL) {
		fprintf(stderr,"Cannot alloc block\n");
		free_vorbis();
		return 0;
	}

	if (vorbis_block_init(ogg_vorbis_dsp_state,ogg_vorbis_block) != 0) {
		fprintf(stderr,"Vorbis block init failed\n");
		free_vorbis();
		return 0;
	}

	wav_stereo = (ogg_vorbis_info->channels >= 2);
	return 1;
}

int mad_more_ogg() {
	ogg_packet pkt={0};

	if (ogg_sync == NULL) init_libogg();
	if (ogg_sync == NULL) return 0;

	wav_16bit = mp3_16bit;
	if (mp3_data_read == mp3_data_write) {
		mp3_data_read = mp3_data;
		mp3_data_write = mp3_data;

		ogg_read_no_loop = 0;
		if (ogg_codec == OGG_VORBIS) {
			float **pcm = NULL;
			int samples = 0,i,c;

			/* TODO: If we see "end of stream" packet and we're done decoding...? */

			if (!next_ogg_packet(&pkt))
				return 0;
			if (vorbis_synthesis(ogg_vorbis_block,&pkt) != 0)
				return 0;
			if (vorbis_synthesis_blockin(ogg_vorbis_dsp_state,ogg_vorbis_block) != 0)
				return 0;
			if ((samples=vorbis_synthesis_pcmout(ogg_vorbis_dsp_state,&pcm)) > 0) {
				if (!mp3_16bit) {
					for (i=0;i < samples;i++) {
						for (c=0;c < (wav_stereo?2:1);c++) {
							int s = (int)(pcm[c][i] * 128);
							if (s > 127) *((int16_t*)mp3_data_write) = 127;
							else if (s < -128) *((int16_t*)mp3_data_write) = -128;
							else *((uint8_t*)mp3_data_write) = (uint16_t)s + 0x80;
							mp3_data_write++;
						}

						if (mp3_data_write >= (mp3_data+sizeof(mp3_data)+2)) {
							fprintf(stderr,"WARNING: Vorbis decode overran buffer, enlarge mp3_data[]\n");
							break;
						}
					}
				}
				else {
					for (i=0;i < samples;i++) {
						for (c=0;c < (wav_stereo?2:1);c++) {
							int s = (int)(pcm[c][i] * 32768);
							if (s > 32767) *((int16_t*)mp3_data_write) = 32767;
							else if (s < -32768) *((int16_t*)mp3_data_write) = -32768;
							else *((int16_t*)mp3_data_write) = (int16_t)s;
							mp3_data_write += 2;
						}

						if (mp3_data_write >= (mp3_data+sizeof(mp3_data)+4)) {
							fprintf(stderr,"WARNING: Vorbis decode overran buffer, enlarge mp3_data[]\n");
							break;
						}
					}
				}

				vorbis_synthesis_read(ogg_vorbis_dsp_state,samples);
			}
		}
		else if (ogg_codec == OGG_SPEEX) {
			SpeexBits sb;
			int ret,i,c;

			if (!next_ogg_packet(&pkt))
				return 0;

			speex_bits_init(&sb);
			speex_bits_read_from(&sb,(char*)pkt.packet,pkt.bytes);
			while ((ret=speex_decode_int(ogg_speex_state,&sb,ogg_speex_out)) == 0) {
				if (!mp3_16bit) {
					for (i=0;i < ogg_speex_frame_size;i++) {
						for (c=0;c < ogg_speex_channels;c++) {
							int s = (int)(ogg_speex_out[c+(i*ogg_speex_channels)] >> 8);
							if (s > 127) *((int16_t*)mp3_data_write) = 127;
							else if (s < -128) *((int16_t*)mp3_data_write) = -128;
							else *((uint8_t*)mp3_data_write) = (uint16_t)s + 0x80;
							mp3_data_write++;
						}

						if (mp3_data_write >= (mp3_data+sizeof(mp3_data)+2)) {
							fprintf(stderr,"WARNING: Vorbis decode overran buffer, enlarge mp3_data[]\n");
							break;
						}
					}
				}
				else {
					for (i=0;i < ogg_speex_frame_size;i++) {
						for (c=0;c < ogg_speex_channels;c++) {
							int s = (int)(ogg_speex_out[c+(i*ogg_speex_channels)]);
							if (s > 32767) *((int16_t*)mp3_data_write) = 32767;
							else if (s < -32768) *((int16_t*)mp3_data_write) = -32768;
							else *((int16_t*)mp3_data_write) = (int16_t)s;
							mp3_data_write += 2;
						}

						if (mp3_data_write >= (mp3_data+sizeof(mp3_data)+4)) {
							fprintf(stderr,"WARNING: Vorbis decode overran buffer, enlarge mp3_data[]\n");
							break;
						}
					}
				}
			}
			speex_bits_destroy(&sb);
		}

		if (mp3_data_read == mp3_data_write)
			return 0;
	}

	return 1;
}

int mad_more() {
	if (qt_reader != NULL)
		return mad_more_qt();
	else if (mp3_file_type == TYPE_MP3)
		return mad_more_mp3();
	else if (mp3_file_type == TYPE_AAC)
		return mad_more_aac();
	else if (mp3_file_type == TYPE_WAV)
		return mad_more_wav();
	else if (mp3_file_type == TYPE_NULL)
		return mad_more_null();
	else if (mp3_file_type == TYPE_OGG)
		return mad_more_ogg();

	return 0;
}

void free_qt() {
	if (qt_reader != NULL)
		qt_reader = quicktime_reader_destroy(qt_reader);
}

void close_mp3() {
	ogg_codec = OGG_NONE;
	free_qt();
	free_speex();
	free_vorbis();
	free_libogg();
	if (mp3_fd >= 0) {
		close(mp3_fd);
		mp3_fd = -1;
	}

	if (aac_esds) free(aac_esds);
	aac_esds_size = 0;
	aac_esds = NULL;

}

int esds_read_desc(unsigned char **pp,int *tag) {
	unsigned char *p = *pp;
	unsigned int len;
	int c;

	*tag = *p++;

	len = 0;
	for (c=0;c < 4;c++) {
		len = (len << 7U) | (*p & 0x7F);
		if (((*p++) & 0x80) == 0) break;
	}

	*pp = p;
	return (int)len;
}

void open_mp3() {
	char *p;
	int i;

	if (mp3_fd < 0) {
		if (strlen(mp3_file) < 1) return;
		mp3_fd = open(mp3_file,O_RDONLY|O_BINARY);
		if (mp3_fd < 0) return;
		mp3_data_offset = 0;
		mp3_data_length = (unsigned long)lseek(mp3_fd,0,SEEK_END);
		lseek(mp3_fd,0,SEEK_SET);
		mp3_sample_rate = 22050;

		if (aac_esds) free(aac_esds);
		aac_esds_size = 0;
		aac_esds = NULL;

		ogg_codec = OGG_NONE;
		free_qt();
		free_speex();
		free_vorbis();
		free_libogg();

		/* do a quick check on the MP3 and determine the sample rate */
		mp3_data_clear();

		/* determine file type from extension */
		p = strrchr(mp3_file,'.');
		if (p == NULL)
			mp3_file_type = TYPE_MP3;
		else if (!strcasecmp(p,".mp4") || !strcasecmp(p,".m4a") || !strcasecmp(p,".m4v") || !strcasecmp(p,".mov"))
			mp3_file_type = TYPE_MP4;
		else if (!strcasecmp(p,".aac"))
			mp3_file_type = TYPE_AAC;
		else if (!strcasecmp(p,".wav"))
			mp3_file_type = TYPE_WAV;
		else if (!strcasecmp(p,".ogg") || !strcasecmp(p,".oga") || !strcasecmp(p,".ogm") || !strcasecmp(p,".spx"))
			mp3_file_type = TYPE_OGG;
		else
			mp3_file_type = TYPE_MP3;

		if (mp3_file_type == TYPE_MP4) {
			size_t buffer_size = 8192;
			unsigned char *buffer;

			free_libmad();
			free_faad2();

			mp3_file_type = TYPE_NULL;
			mp3_sample_rate = 22050;
			mp3_stereo = 0;
			mp3_16bit = 1;
			wav_16bit = 1;

			qt_audio_chunk = 0;
			qt_audio_codec = -1;
			qt_audio_index = -1;
			qt_audio_sample = 0;
			if ((qt_reader = quicktime_reader_create()) == NULL) {
				fprintf(stderr,"Cannot create QT reader\n");
				free_qt();
				return;
			}
			quicktime_reader_be_verbose(qt_reader);
			if (!quicktime_reader_assign_fd(qt_reader,mp3_fd)) {
				fprintf(stderr,"Cannot assign reader to file\n");
				free_qt();
				return;
			}
			if (!quicktime_reader_parse(qt_reader)) {
				fprintf(stderr,"Cannot open quicktime_reader_parse() file\n");
				free_qt();
				return;
			}
			if (qt_reader->max_track <= 0) {
				fprintf(stderr,"No tracks in the quicktime file AT ALL\n");
				free_qt();
				return;
			}

			buffer = malloc(buffer_size);
			if (buffer != NULL) {
				for (i=0;i < qt_reader->max_track && qt_audio_index < 0;i++) {
					quicktime_reader_track *trk = qt_reader->track + i;
//					quicktime_reader_stsc_entry *ent = NULL;

					if (__be_u32(&trk->mhlr_header.component_subtype) == quicktime_type_const('s','o','u','n')) {
						unsigned char *p = (unsigned char *)buffer;
						unsigned char *fence = p + buffer_size - 8;
						quicktime_stsd_header *h = (quicktime_stsd_header*)buffer;
						unsigned long ent,max;

						quicktime_stack_empty(qt_reader->r_moov.qt);
						quicktime_stack_push(qt_reader->r_moov.qt,&trk->stsd);
						memset(buffer,0,buffer_size);
						quicktime_stack_read(qt_reader->r_moov.qt,
							quicktime_stack_top(qt_reader->r_moov.qt),buffer,buffer_size);
						quicktime_stack_empty(qt_reader->r_moov.qt);

						max = __be_u32(&h->number_of_entries);
						p += sizeof(*h);

						for (ent=0;ent < max && (p+32) < fence;ent++) {
							quicktime_stsd_entry_general *en;
							quicktime_stsd_entry_audio *aen;

							en = (quicktime_stsd_entry_general*)p;
							p += __be_u32(&en->size);

							/* must be "mp4a" */
							if (__be_u32(&en->data_format) == quicktime_type_const('m','p','4','a')) {
								unsigned char *esds = NULL,*wave = NULL;
								unsigned int hsz = 0x24;
								unsigned char *es;

								qt_audio_index = i;
								aen = (quicktime_stsd_entry_audio*)en;
								mp3_sample_rate = __be_u32(&aen->sample_rate) >> 16UL;
								wav_stereo = mp3_stereo = __be_u16(&aen->number_of_channels) >= 2;

								qt_audio_codec = TYPE_AAC;
								mp3_file_type = TYPE_AAC;

								/* v1 structs are slightly larger */
								if (__be_u16(&aen->version) == 1) {
									fprintf(stderr,"Version 1 audio\n");
									hsz += 16;
								}

								/* look for the 'esds' it contains all the info libfaad needs to
								 * parse the AAC audio.
								 *
								 * Apparently:
								 *   - ISO MPEG-4 *.mp4 files: It's right there in an 'esds' chunk.
								 *   - Quicktime *.mov files: It's in 'wave' -> 'esds' */
								es = (unsigned char*)en + hsz;
								while ((es+8) <= p) {
									unsigned char *payload = es+8;
									uint32_t id = *((uint32_t*)(es+4));
									uint32_t len = __be_u32(es+0);
									if (len >= 65536) break;
									if ((es+len) > p) break;
									es += len;

									if (!memcmp(&id,"esds",4) && len >= 12 && len <= 512) {
										esds = payload;
									}
									else if (!memcmp(&id,"wave",4) && len >= 12 && len <= 2048) {
										wave = payload;
									}
								}
								
								if (esds == NULL && wave != NULL) {
									es = (unsigned char*)wave;
									while ((es+8) <= p) {
										unsigned char *payload = es+8;
										uint32_t id = *((uint32_t*)(es+4));
										uint32_t len = __be_u32(es+0);
										if (len >= 65536) break;
										if ((es+len) > p) break;
										es += len;

										if (!memcmp(&id,"esds",4) && len >= 12 && len <= 512) {
											esds = payload;
										}
									}
								}

								if (esds != NULL) {
									unsigned char *x = esds;
									int tag,len;

									/* now we have to hunt for it through the most
									 * illogical fucked up "tagging" encoding I've
									 * ever seen come out of the MPEG standards.
									 *
									 * Seriously gus, what were you smoking? */
									x += 4; /* skip version + flags */

									len = esds_read_desc(&x,&tag);
									if (tag == 3) { /* MP4ESDescrTag */
										unsigned char flags;

										/* Notice that in most MP4 files I've seen
										 * the length will be something like 0x22
										 * or at least the remaining bytecount of
										 * the esds tag... except that the length
										 * is really meaningless and you parse instead
										 * a fixed amount of bytes for this tag.
										 *
										 * Right, so what's the fucking point of
										 * the encoded length, then? Hm? */
										x += 2; /* ES id (?) */
										flags = *x++;
										if (flags & 0x80)
											x += 2;	/* streamDependenceFlag */
										if (flags & 0x40) { /* URL flag---wait..what? */
											unsigned char len = *x++;
											x += len;
										}
										if (flags & 0x20)
											x += 2; /* OCRstreamFlag */
									}
									else {
										x += 2; /* Just the ID... I guess */
									}

									len = esds_read_desc(&x,&tag);
									if (tag == 4) { /* MP4DecConfigDescrTag */
										/* Once again! The length is encoded but
										 * has *NO MEANING*. Why did you even bother
										 * to put the length in there?!?
										 *
										 * What were you people THINKING?!? */
										x++; /* object type ID (0x40 = AAC?) */
										x++; /* stream type */
										x += 3; /* buffer size db */
										x += 4; /* max bitrate */
										x += 4; /* avg bitrate */

										/* the length of the field we really want
										 * amounts to two bytes. Yes, we're doing
										 * this needlessly complex scan for TWO
										 * STUPID BYTES. This is the kind of bullshit
										 * I'd expect from a Microsoft standard,
										 * not the work of the MPEG committee. */
										len = esds_read_desc(&x,&tag);
										if (len != 0 && len < 64) {
											if (!aac_esds) {
												aac_esds_size = len+4;
												aac_esds = malloc(aac_esds_size);
												if (aac_esds) memcpy(aac_esds,x,aac_esds_size);
												x += aac_esds_size;
											}
										}
									}
								}

								break;
							}
						}
					}
				}

				free(buffer);
			}
		}
		else if (mp3_file_type == TYPE_WAV) {
			mp3_data_clear();
			mp3_data_refill();

			/* look into the buffer and examine the RIFF wave header */
			mp3_sample_rate = 22050;
			mp3_stereo = 0;
			mp3_16bit = 0;

			if (!memcmp(mp3_data_read,"RIFF",4) && !memcmp(mp3_data_read+8,"WAVE",4)) {
				unsigned long ofs = 0;

				/* NTS: the buffer is 8-16KB and most WAVE files have the fmt and data header
				 *      within the first 44 bytes. even if we end up with the invalid read > write
				 *      pointer scenario we still reside within the buffer and no memory corruption
				 *      shall occur. */
				/* bytes 4-7 are the length of the whole RIFF file. Ignore it */
				mp3_data_read += 12; /* skip RIFF:WAVE header and locate the 'fmt ' chunk */
				
				while ((mp3_data_read+8) <= mp3_data_write) {
					uint32_t id = *((uint32_t*)(mp3_data_read+0));
					uint32_t len = *((uint32_t*)(mp3_data_read+4));
					if ((mp3_data_read += 8) >= mp3_data_write) break;

					/* I apologize for this hack: it's very clever and unorthodox */
					if (memcmp((char*)(&id),"data",4) == 0) {
						mp3_data_offset = ofs + ((unsigned long)(mp3_data_read - mp3_data));
						mp3_data_length = len;
						break;
					}
					else if (memcmp((char*)(&id),"fmt ",4) == 0) {
						windows_WAVEFORMAT *wfx;

						if (len >= 65536) break;
						else if ((mp3_data_read+len) > mp3_data_write) break;
						wfx = (windows_WAVEFORMAT*)(mp3_data_read);

						if (wfx->wFormatTag == 1) {
							mp3_sample_rate = wfx->nSamplesPerSec;
							mp3_stereo = (wfx->nChannels > 1);
							mp3_16bit = (wfx->wBitsPerSample >= 16);
						}
						else {
							/* it's a codec we don't support, so invoke the NULL output */
							mp3_file_type = TYPE_NULL;
						}

						mp3_data_read += ((len + 1UL) & (~1UL));
					}
					else {
						/* some other chunk */
						if (len >= 65536) break;
						else if ((mp3_data_read+len) > mp3_data_write) break;
						mp3_data_read += ((len + 1UL) & (~1UL));
					}
				}
			}
			else {
				/* then let them play it raw */
			}

			mp3_smallstep = mp3_sample_rate * (mp3_stereo ? 2 : 1) * (mp3_16bit ? 2 : 1) * 5;
			wav_stereo = mp3_stereo;
			wav_16bit = mp3_16bit;
		}
		else if (mp3_file_type == TYPE_MP3) {
			do {
				mp3_data_flush(0);
				mp3_data_refill();
				if ((mp3_data_read+64) >= mp3_data_write) break;

				mad_stream_buffer(&mad_stream,mp3_data_read,(size_t)(mp3_data_write - mp3_data_read));
				mad_stream.sync = 0;
				mad_stream.skiplen = 0;
				mad_header_decode(&mad_frame.header,&mad_stream);

				if (mad_stream.sync) {
					mp3_sample_rate = mad_frame.header.samplerate;
					break;
				}
				else if (mad_stream.next_frame != NULL) {
					mp3_data_read = (unsigned char*)mad_stream.next_frame;
				}
				else {
					mp3_data_read += 4;
				}
			} while (1);

			mp3_smallstep = 128*1024;
		}
		else if (mp3_file_type == TYPE_AAC) {
			do {
				mp3_data_flush(0);
				mp3_data_refill();
				if ((mp3_data_read+64) >= mp3_data_write) break;

				mp3_data_read = mpeg_aac_adts_audio_sync(mp3_data_read,mp3_data_write,&aac_nsync,&aac_ninfo);
				if (aac_nsync != 0ULL && aac_ninfo.sample_rate != 0) {
					mp3_sample_rate = aac_ninfo.sample_rate;
					/* FAAD2 will double the sample rate in case of SBR HE-AAC */
					if (mp3_sample_rate < 32000) mp3_sample_rate *= 2;
					aac_info = aac_ninfo;
					aac_sync = aac_nsync;
					break;
				}
				else {
					mp3_data_read += 4;
				}
			} while (1);

			mp3_smallstep = 96*1024;
		}
		else if (mp3_file_type == TYPE_OGG) {
			wav_16bit = 1;
			if (init_libogg()) {
				ogg_packet pkt={0};

				ogg_codec = 0;
				ogg_read_no_loop = 1;
				while (ogg_codec == 0 && next_ogg_page()) {
					/* we're looking for a stream start */
					if (ogg_page_bos(ogg_sync_page)) {
						ogg_sync_stream = (ogg_stream_state*)malloc(sizeof(ogg_stream_state));
						if (ogg_sync_stream != NULL) {
							int ok = 0;

							memset(ogg_sync_stream,0,sizeof(*ogg_sync_stream));
							if (ogg_stream_init(ogg_sync_stream,ogg_page_serialno(ogg_sync_page)) == 0 &&
								ogg_stream_pagein(ogg_sync_stream,ogg_sync_page) == 0 && next_ogg_packet(&pkt)) { /* NTS: Apparently "b_o_s" is not set for the packet */
								ok = ogg_known_codec(&pkt);
							}

							if (ok == OGG_VORBIS) {
								if (init_vorbis(&pkt)) {
									mp3_sample_rate = ogg_vorbis_info->rate;
									mp3_stereo = (ogg_vorbis_info->channels >= 2);
								}
								else {
									ok = 0;
								}
							}
							else if (ok == OGG_SPEEX) {
								if (init_speex(&pkt)) {
									mp3_sample_rate = ogg_speex_rate;
									mp3_stereo = (ogg_speex_channels >= 2);
								}
								else {
									ok = 0;
								}
							}

							ogg_codec = ok;
							if (!ok) {
								fprintf(stderr,"Failed to lock on. Sig: %u %s\n",pkt.packet[0],pkt.packet);
								ogg_stream_clear(ogg_sync_stream);
								free(ogg_sync_stream);
								ogg_sync_stream = NULL;
							}
						}
					}
				}
			}

			mp3_smallstep = 144*1024;
			wav_stereo = mp3_stereo;
		}

		mp3_bytes_per_sample = (mp3_stereo ? 2 : 1) * (mp3_16bit ? 2 : 1);
		lseek(mp3_fd,mp3_data_offset,SEEK_SET);
		mp3_data_clear();
	}
}

#if !defined(TARGET_WINDOWS)
void vga_write_until(unsigned int x) {
	while (vga_state.vga_pos_x < x)
		vga_writec(' ');
}
#endif

void mad_reset_decoder() {
	free_libmad();
	free_faad2();

	if (qt_reader) {
		quicktime_reader_track *trk = qt_reader->track + qt_audio_index;
		quicktime_reader_stsc_entry *ent;
		unsigned long ofs,chofs,saml,sample;
		int sam,found=0;

		/* the QuickTime reader forces the file pointer around according to which
		 * QuickTime chunk it is currently reading from. So to make random-access
		 * work we need to take the file pointer and locate the nearest chunk. */
		ofs = lseek(mp3_fd,0,SEEK_CUR);

		qt_audio_chunk = 0;
		qt_audio_sample = 0;
		while (!found && qt_audio_chunk < trk->chunk.total) {
			ent = trk->sampletochunk.list + qt_audio_chunk;
			chofs = trk->chunk.list[qt_audio_chunk];
			sample = ent->first_sample;

			for (sam=0;!found && sam < ent->samples_per_chunk;sam++,sample++) {
				if (trk->samplesize.list == NULL || sample >= trk->samplesize.entries) break;

				saml = trk->samplesize.list[sample];
				if (ofs >= chofs && ofs < (chofs+saml)) {
					found = 1;
					break;
				}

				chofs += saml;
			}

			if (!found) qt_audio_chunk++;
		}

		if (qt_audio_chunk >= trk->chunk.total)
			qt_audio_chunk = 0;
	}

	if (mp3_file_type == TYPE_MP3)
		init_libmad();
	else if (mp3_file_type == TYPE_AAC)
		init_faad2();
	else if (mp3_file_type == TYPE_OGG) {
		init_libogg();
		if (ogg_sync) ogg_sync_reset(ogg_sync);
		if (ogg_sync_stream) ogg_stream_reset(ogg_sync_stream);
	}

	if (mp3_file_type == TYPE_WAV || mp3_file_type == TYPE_NULL || mp3_file_type == TYPE_OGG) {
		/* do not fill, just clear. the loading code will take care of it */
		mp3_data_clear();
	}
	else {
		mp3_data_clear();
		mp3_data_refill();
	}
}

int confirm_quit() {
#if defined(TARGET_WINDOWS)
	/* TODO */
	return 1;
#else
	/* FIXME: Why does this cause Direct DSP playback to horrifically slow down? */
	return confirm_yes_no_dialog("Are you sure you want to exit to DOS?");
#endif
}

void prompt_play_mp3(unsigned char rec) {
#if defined(TARGET_WINDOWS)
	char tmp[300];
	OPENFILENAME of;

	memset(&of,0,sizeof(of));
	of.lStructSize = sizeof(of);
	of.hwndOwner = hwndMain;
	of.hInstance = myInstance;
	of.lpstrFilter =
		"All supported files\x00*.mp3;*.mp2;*.mpa;*.wav;*.aac;*.ogg;*.oga;*.spx;*.mp4;*.m4a;*.m4v;*.mov;*.qt\x00"
		"MP3 files\x00*.mp3;*.mp2;*.mpa\x00"
		"WAV files\x00*.wav\x00"
		"AAC files\x00*.aac\x00"
		"Ogg Vorbis files\x00*.ogg;*.oga\x00"
		"Ogg Speex files\x00*.spx\x00"
		"MPEG-4 audio\x00*.mp4;*.m4a;*.m4v;*.mov;*.qt\x00"
		"All files\x00*.*\x00";
	of.nFilterIndex = 1;
	strcpy(tmp,mp3_file);
	of.lpstrFile = tmp;
	of.nMaxFile = sizeof(tmp)-1;
	of.lpstrTitle = "Select file to play";
	of.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	if (GetOpenFileName(&of)) {
		unsigned char wp = mp3_playing;
		stop_play();
		close_mp3();
		strcpy(mp3_file,tmp);
		open_mp3();
		if (wp) begin_play();
	}
#else
	unsigned char gredraw = 1;
	struct find_t ft;

	{
		const char *rp;
		char temp[sizeof(mp3_file)];
		int cursor = strlen(mp3_file),i,c,redraw=1,ok=0;
		memcpy(temp,mp3_file,strlen(mp3_file)+1);
		while (!ok) {
			if (gredraw) {
				char *cwd;

				gredraw = 0;
				vga_clear();
				vga_moveto(0,4);
				vga_write_color(0x07);
				vga_write("Enter file path:\n");
				vga_write_sync();
				draw_irq_indicator();
				ui_anim(1);
				redraw = 1;

				cwd = getcwd(NULL,0);
				if (cwd) {
					vga_moveto(0,6);
					vga_write_color(0x0B);
					vga_write(cwd);
					vga_write_sync();
				}

				if (_dos_findfirst("*.*",_A_NORMAL|_A_RDONLY,&ft) == 0) {
					int x=0,y=7,cw = 14,i;
					char *ex;

					do {
						ex = strrchr(ft.name,'.');
						if (!ex) ex = "";

						if (ft.attrib&_A_SUBDIR) {
							vga_write_color(0x0F);
						}
						else if (!strcasecmp(ex,".wav") || !strcasecmp(ex,".mp3") || !strcasecmp(ex,".mp4") ||
							!strcasecmp(ex,".m4a") || !strcasecmp(ex,".mp2") || !strcasecmp(ex,".ac3") ||
							!strcasecmp(ex,".aac") || !strcasecmp(ex,".ogg") || !strcasecmp(ex,".spx")) {
							vga_write_color(0x1E);
						}
						else {
							vga_write_color(0x07);
						}
						vga_moveto(x,y);
						for (i=0;i < 13 && ft.name[i] != 0;) vga_writec(ft.name[i++]);
						for (;i < 14;i++) vga_writec(' ');

						x += cw;
						if ((x+cw) > vga_state.vga_width) {
							x = 0;
							if (y >= vga_state.vga_height) break;
							y++;
						}
					} while (_dos_findnext(&ft) == 0);

					_dos_findclose(&ft);
				}
			}
			if (redraw) {
				rp = (const char*)temp;
				vga_moveto(0,5);
				vga_write_color(0x0E);
				for (i=0;i < 80;i++) {
					if (*rp != 0)	vga_writec(*rp++);
					else		vga_writec(' ');	
				}
				vga_moveto(cursor,5);
				vga_write_sync();
				redraw=0;
			}

			if (kbhit()) {
				c = getch();
				if (c == 0) c = getch() << 8;

				if (c == 27) {
					ok = -1;
				}
				else if (c == 13) {
#if TARGET_MSDOS == 16 && (defined(__COMPACT__) || defined(__SMALL__))
					ok = 1;
#else
					struct stat st;

					if (isalpha(temp[0]) && temp[1] == ':' && temp[2] == 0) {
						unsigned int total;

						_dos_setdrive(tolower(temp[0])+1-'a',&total);
						temp[0] = 0;
						gredraw = 1;
						cursor = 0;
					}
					else if (stat(temp,&st) == 0) {
						if (S_ISDIR(st.st_mode)) {
							chdir(temp);
							temp[0] = 0;
							gredraw = 1;
							cursor = 0;
						}
						else {
							ok = 1;
						}
					}
					else {
						ok = 1;
					}
#endif
				}
				else if (c == 8) {
					if (cursor != 0) {
						temp[--cursor] = 0;
						redraw = 1;
					}
				}
				else if (c >= 32 && c < 256) {
					if (cursor < 79) {
						temp[cursor++] = (char)c;
						temp[cursor  ] = (char)0;
						redraw = 1;
					}
				}
			}
			else {
				ui_anim(0);
			}
		}

		if (ok == 1) {
			unsigned char wp = mp3_playing;
			stop_play();
			close_mp3();
			memcpy(mp3_file,temp,strlen(temp)+1);
			open_mp3();
			if (wp) begin_play();
		}
	}
#endif
}

