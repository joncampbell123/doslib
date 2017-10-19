
/* NTS: Unlike GCC, Open Watcom doesn't cue into the declaration as "static const" that
 *      it can optimize the code by baking the static const into the immediate form of
 *      x86 instructions. Therefore optimization of that form requires the use of
 *      #define macros instead. Making Open Watcom allocate memory for the constants
 *      and then use the memory locations for what could be optimized slows this code
 *      down. */
#if TARGET_MSDOS == 32
# define resample_100_shift             (32)
# define resample_100                   (1ULL << 32ULL)
typedef signed long long                resample_intermediate_t;
typedef unsigned long                   resample_counting_element_t; /* one half of the fraction (size of a CPU register, usually) */
typedef unsigned long long              resample_whole_count_element_t; /* whole + fraction, fixed pt */
#else
# define resample_100_shift             (16)
# define resample_100                   (1UL << 16UL)
typedef signed long                     resample_intermediate_t;
typedef unsigned int                    resample_counting_element_t; /* one half of the fraction (size of a CPU register, usually) */
typedef unsigned long                   resample_whole_count_element_t; /* whole + fraction, fixed pt */
#endif

#define resample_max_channels           (2)

/* resampler state */
struct resampler_state_t {
    resample_whole_count_element_t      step; /* fixed point step (where 1.0 == resample_100) */
    resample_whole_count_element_t      frac;
    int16_t                             p[resample_max_channels];
    int16_t                             c[resample_max_channels];
    unsigned int                        init:1;
};

extern struct resampler_state_t         resample_state;

extern unsigned char                    resample_on;

static inline int resample_interpolate_generic(const unsigned int channel) {
    resample_intermediate_t tmp;

    tmp = (resample_intermediate_t)resample_state.c[channel] - (resample_intermediate_t)resample_state.p[channel];
    tmp *= (resample_intermediate_t)resample_state.frac;
    tmp >>= (resample_intermediate_t)resample_100_shift;
    tmp += resample_state.p[channel];

    return (int)tmp;
}

void resampler_state_reset(struct resampler_state_t *r);
int resampler_init(struct resampler_state_t *r,struct wav_cbr_t * const d,const struct wav_cbr_t * const s);

