
extern FILE*                            report_fp;

extern struct dma_8237_allocation*      sb_dma; /* DMA buffer */

extern struct sndsb_ctx*                sb_card;

extern unsigned char far                devnode_raw[4096];

extern uint32_t                         buffer_limit;

extern char                             ptmp[256];

extern unsigned int                     wav_sample_rate;
extern unsigned char                    wav_stereo;
extern unsigned char                    wav_16bit;

extern unsigned char                    old_irq_masked;
extern void                             (interrupt *old_irq)();

unsigned char sndsb_encode_adpcm_2bit(unsigned char samp);
unsigned char sndsb_encode_adpcm_4bit(unsigned char samp);
unsigned char sndsb_encode_adpcm_2_6bit(unsigned char samp,unsigned char b2);
void sndsb_encode_adpcm_set_reference(unsigned char c,unsigned char mode);

void free_dma_buffer();
void interrupt sb_irq();
void realloc_dma_buffer();
void generate_1khz_sine_adpcm2(void);
void generate_1khz_sine_adpcm26(void);
void generate_1khz_sine_adpcm4(void);
void generate_1khz_sine(void);
void generate_1khz_sine16(void);
void generate_1khz_sine16s(void);
void doubleprintf(const char *fmt,...);
int common_sb_init(void);

