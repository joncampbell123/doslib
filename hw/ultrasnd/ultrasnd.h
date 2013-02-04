
#include <hw/cpu/cpu.h>
#include <stdint.h>

/* 2 seems a reasonable max, since 1 is most common */
#define MAX_ULTRASND				2

/* maximum sample rate (at 14 or less voices) */
#define ULTRASND_RATE				44100

#define ULTRASND_VOICE_MODE_IS_STOPPED		0x01
#define ULTRASND_VOICE_MODE_STOP		0x02
#define ULTRASND_VOICE_MODE_16BIT		0x04
#define ULTRASND_VOICE_MODE_LOOP		0x08
#define ULTRASND_VOICE_MODE_BIDIR		0x10
#define ULTRASND_VOICE_MODE_IRQ			0x20
#define ULTRASND_VOICE_MODE_BACKWARDS		0x40
#define ULTRASND_VOICE_MODE_IRQ_PENDING		0x80

/* data being sent is 16-bit PCM */
#define ULTRASND_DMA_DATA_SIZE_16BIT		0x40
/* during transfer invert bit 7 (or bit 15) to convert unsigned->signed */
#define ULTRASND_DMA_FLIP_MSB			0x80

struct ultrasnd_ctx {
	int16_t		port;		/* NOTE: Gravis ultrasound takes port+0x0 to port+0xF, and port+0x100 to port+0x10F */
	int8_t		dma1,dma2;	/* NOTE: These can be the same */
	int8_t		irq1,irq2;	/* NOTE: These can be the same */
	uint32_t	total_ram;	/* total RAM onboard. in most cases, a multiple of 256KB */
	uint8_t		active_voices;
	uint16_t	output_rate;
	uint8_t		boundary256k:1;	/* whether all DMA transfers, playback, etc. must occur within 256KB block boundaries */
	uint8_t		use_dma:1;	/* whether to use DMA to upload/download data from DRAM */
	uint8_t		reserved:7;
	uint8_t		voicemode[32];	/* voice modes. because when you enable 16-bit PCM weird shit happens to the DRAM addresses */
	struct dma_8237_allocation *dram_xfer_a;
};

extern const uint16_t ultrasnd_rate_per_voices[33];
extern struct ultrasnd_ctx ultrasnd_card[MAX_ULTRASND];
extern struct ultrasnd_ctx *ultrasnd_env;
extern int ultrasnd_inited;

void ultrasnd_debug(int on);
void ultrasnd_select_voice(struct ultrasnd_ctx *u,uint8_t reg);
void ultrasnd_select_write(struct ultrasnd_ctx *u,uint8_t reg,uint8_t data);
void ultrasnd_select_write16(struct ultrasnd_ctx *u,uint8_t reg,uint16_t data);
uint16_t ultrasnd_select_read16(struct ultrasnd_ctx *u,uint8_t reg);
uint8_t ultrasnd_select_read(struct ultrasnd_ctx *u,uint8_t reg);
uint8_t ultrasnd_selected_read(struct ultrasnd_ctx *u);
unsigned char ultrasnd_peek(struct ultrasnd_ctx *u,uint32_t ofs);
void ultrasnd_poke(struct ultrasnd_ctx *u,uint32_t ofs,uint8_t b);
int ultrasnd_valid_dma(struct ultrasnd_ctx *u,int8_t i);
int ultrasnd_valid_irq(struct ultrasnd_ctx *u,int8_t i);
void ultrasnd_set_active_voices(struct ultrasnd_ctx *u,unsigned char voices);
int ultrasnd_probe(struct ultrasnd_ctx *u,int program_cfg);
int ultrasnd_irq_taken(int irq);
int ultrasnd_dma_taken(int dma);
int ultrasnd_port_taken(int port);
void ultrasnd_init_ctx(struct ultrasnd_ctx *u);
void ultrasnd_free_card(struct ultrasnd_ctx *u);
int ultrasnd_card_taken(struct ultrasnd_ctx *u);
int init_ultrasnd();
struct ultrasnd_ctx *ultrasnd_alloc_card();
int ultrasnd_test_irq_timer(struct ultrasnd_ctx *u,int irq);
struct ultrasnd_ctx *ultrasnd_try_base(uint16_t base);
struct ultrasnd_ctx *ultrasnd_try_ultrasnd_env();
uint16_t ultrasnd_sample_rate_to_fc(struct ultrasnd_ctx *u,unsigned int r);
unsigned char ultrasnd_read_voice_mode(struct ultrasnd_ctx *u,unsigned char voice);
void ultrasnd_set_voice_mode(struct ultrasnd_ctx *u,unsigned char voice,uint8_t mode);
void ultrasnd_set_voice_fc(struct ultrasnd_ctx *u,unsigned char voice,uint16_t fc);
void ultrasnd_set_voice_start(struct ultrasnd_ctx *u,unsigned char voice,uint32_t ofs);
void ultrasnd_set_voice_end(struct ultrasnd_ctx *u,unsigned char voice,uint32_t ofs);
void ultrasnd_stop_voice(struct ultrasnd_ctx *u,int i);
void ultrasnd_start_voice(struct ultrasnd_ctx *u,int i);
void ultrasnd_start_voice_imm(struct ultrasnd_ctx *u,int i);
void ultrasnd_set_voice_ramp_rate(struct ultrasnd_ctx *u,unsigned char voice,unsigned char adj,unsigned char rate);
void ultrasnd_set_voice_ramp_start(struct ultrasnd_ctx *u,unsigned char voice,unsigned char start);
void ultrasnd_set_voice_ramp_end(struct ultrasnd_ctx *u,unsigned char voice,unsigned char end);
void ultrasnd_set_voice_volume(struct ultrasnd_ctx *u,unsigned char voice,uint16_t vol);
uint32_t ultrasnd_read_voice_current(struct ultrasnd_ctx *u,unsigned char voice);
void ultrasnd_set_voice_current(struct ultrasnd_ctx *u,unsigned char voice,uint32_t loc);
void ultrasnd_set_voice_pan(struct ultrasnd_ctx *u,unsigned char voice,uint8_t pan);
void ultrasnd_set_voice_ramp_control(struct ultrasnd_ctx *u,unsigned char voice,uint8_t ctl);
unsigned char FAR *ultrasnd_dram_buffer_alloc(struct ultrasnd_ctx *u,unsigned long len);
int ultrasnd_send_dram_buffer(struct ultrasnd_ctx *u,uint32_t ofs,unsigned long len,uint8_t flags);
void ultrasnd_dram_buffer_free(struct ultrasnd_ctx *u);
