
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

