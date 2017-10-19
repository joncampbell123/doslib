
struct convert_rdbuf_t {
    unsigned char dosamp_FAR *          buffer; // pointer to base
    unsigned int                        size;   // size in bytes (16-bit builds will not exceed 64KN)
    unsigned int                        len;    // length of actual data
    unsigned int                        pos;    // read position of data
};

extern struct convert_rdbuf_t           convert_rdbuf;

unsigned char dosamp_FAR * convert_rdbuf_get(uint32_t *sz);
void convert_rdbuf_check(void);
void convert_rdbuf_clear(void);
void convert_rdbuf_free(void);

uint32_t convert_rdbuf_resample_to_8_mono(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_to_8_stereo(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_to_16_mono(int16_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_to_16_stereo(int16_t dosamp_FAR *dst,uint32_t samples);

uint32_t convert_rdbuf_resample_fast_to_8_mono(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_fast_to_8_stereo(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_fast_to_16_mono(int16_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_fast_to_16_stereo(int16_t dosamp_FAR *dst,uint32_t samples);

uint32_t convert_rdbuf_resample_best_to_8_mono(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_best_to_8_stereo(uint8_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_best_to_16_mono(int16_t dosamp_FAR *dst,uint32_t samples);
uint32_t convert_rdbuf_resample_best_to_16_stereo(int16_t dosamp_FAR *dst,uint32_t samples);

