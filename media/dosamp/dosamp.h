
/* making the API use FAR CALL function pointers will allow future development
 * where file access and media playback code can exist in external DLLs, whether
 * 16-bit or 32-bit code */
#if TARGET_MSDOS == 32 && !defined(WIN386)
# define dosamp_FAR
#elif defined(LINUX)
# define dosamp_FAR
#else
# define dosamp_FAR far
#endif

/* platform has IRQ handling */
#if defined(LINUX) || defined(TARGET_WINDOWS) || defined(TARGET_OS2)
/* no */
#else
# define HAS_IRQ
#endif

/* platform has DMA handling */
#if defined(LINUX) || defined(TARGET_WINDOWS) || defined(TARGET_OS2)
/* no */
#else
# define HAS_DMA
#endif

/* platform has 8042 timer */
#if defined(LINUX)
/* no */
#elif defined(TARGET_WINDOWS)
/* Windows 3.x MIGHT have WINMM multimedia timer, might not.
 * If it doesn't, we can do what any other Windows 3.x app would do
 * and just talk directly to the 8042, because Windows 3.x/9x/ME
 * is that kind of platform where IO is allowed in userspace (but
 * sometimes virtualized by kernel drivers). Note that direct IO
 * of this kind is a no-no in 32-bit Windows NT, but we'll detect
 * that case and avoid the code then. If we're a 16-bit WINAPP,
 * we *can* do this in Windows NT because NTVDM.EXE will trap and
 * emulate the 8042 timer. */
# define HAS_8254
#else
# define HAS_8254
#endif

/* platform should use winfcon (our own DOSLIB console emulation) */
#if defined(TARGET_WINDOWS) && TARGET_WINDOWS < 40
/* yes: Windows 3.x does not offer a console even for Win32s applications */
# define USE_WINFCON
#else
/* no */
#endif

/* platform has RDTSC timer */
#if defined(LINUX)
/* no */
#else
# define HAS_RDTSC
#endif

/* platform has Sound Blaster direct access */
#if defined(LINUX) || defined(TARGET_WINDOWS) || defined(TARGET_OS2)
/* no */
#else
# define HAS_SNDSB
#endif

/* platform has CLI/STI */
#if defined(LINUX) || defined(TARGET_WINDOWS) || defined(TARGET_OS2)
/* no */
#else
# define HAS_CLISTI
#endif

/* platform has clock_gettime() and CLOCK_MONOTONIC */
#if defined(LINUX)
# define HAS_CLOCK_MONOTONIC
#else
/* no */
#endif

/* platform has OSS */
#if defined(LINUX)
# define HAS_OSS
#else
/* no */
#endif

/* platform has ALSA */
#if defined(LINUX)
# define HAS_ALSA
#else
/* no */
#endif

#ifdef USE_WINFCON
# include <hw/dos/winfcon.h>
#endif

struct wav_cbr_t {
    uint32_t                                    sample_rate;
    uint16_t                                    bytes_per_block;
    uint16_t                                    samples_per_block;
    uint8_t                                     number_of_channels; /* nobody's going to ask us to play 4096 channel-audio! */
    uint8_t                                     bits_per_sample;    /* nor will they ask us to play 512-bit PCM audio! */
};

extern struct wav_cbr_t                         file_codec;
extern struct wav_cbr_t                         play_codec;

struct wav_state_t {
    uint32_t                                    dma_position;
    uint32_t                                    play_delay_bytes;/* in bytes. delay from wav_position to where sound card is playing now. */
    uint32_t                                    play_delay;/* in samples. delay from wav_position to where sound card is playing now. */
    uint64_t                                    write_counter;
    uint64_t                                    play_counter;
    uint64_t                                    play_counter_prev;
    unsigned int                                play_empty:1;
    unsigned int                                prepared:1;
    unsigned int                                playing:1;
    unsigned int                                is_open:1;
};

void wav_reset_state(struct wav_state_t dosamp_FAR * const w);

extern char                                     str_tmp[256];
extern char                                     soundcard_str_tmp[256];

