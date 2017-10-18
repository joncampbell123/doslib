
#pragma pack(push,1)
typedef struct windows_WAVEFORMATPCM {
    uint16_t    wFormatTag;             /* +0 */
    uint16_t    nChannels;              /* +2 */
    uint32_t    nSamplesPerSec;         /* +4 */
    uint32_t    nAvgBytesPerSec;        /* +8 */
    uint16_t    nBlockAlign;            /* +12 */
    uint16_t    wBitsPerSample;         /* +14 */
} windows_WAVEFORMATPCM;                /* =16 */
#pragma pack(pop)

#define windows_WAVE_FORMAT_PCM         0x0001

