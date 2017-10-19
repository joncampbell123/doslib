
/* making the API use FAR CALL function pointers will allow future development
 * where file access and media playback code can exist in external DLLs, whether
 * 16-bit or 32-bit code */
#if TARGET_MSDOS == 32 && !defined(WIN386)
# define dosamp_FAR
#else
# define dosamp_FAR far
#endif

struct wav_cbr_t {
    uint32_t                            sample_rate;
    uint16_t                            bytes_per_block;
    uint16_t                            samples_per_block;
    uint8_t                             number_of_channels; /* nobody's going to ask us to play 4096 channel-audio! */
    uint8_t                             bits_per_sample;    /* nor will they ask us to play 512-bit PCM audio! */
};

extern struct wav_cbr_t                 file_codec;
extern struct wav_cbr_t                 play_codec;

