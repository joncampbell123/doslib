
/* sound card state */
enum soundcard_drv_t {
    soundcard_none=0,
    soundcard_soundblaster
};

struct soundcard_priv_soundblaster_t {
    int8_t                                      index;          /* array into sndsb library card array */
};

struct soundcard;
typedef struct soundcard dosamp_FAR * soundcard_t;
typedef struct soundcard dosamp_FAR * dosamp_FAR * soundcard_ptr_t;

#define soundcard_caps_8bit                     (1U << 0U)
#define soundcard_caps_16bit                    (1U << 1U)
#define soundcard_caps_mono                     (1U << 2U)
#define soundcard_caps_sterep                   (1U << 3U)
#define soundcard_caps_mmap_write               (1U << 4U)
#define soundcard_caps_irq                      (1U << 5U)      /* uses/can use an IRQ */
#define soundcard_caps_isa_dma                  (1U << 6U)      /* uses/can use ISA DMA */

#define soundcard_requirements_isa_dma          (1U << 0U)      /* requires ISA DMA */
#define soundcard_requirements_irq              (1U << 1U)      /* requires IRQ */

struct soundcard {
    enum soundcard_drv_t                        driver;
    uint16_t                                    capabilities;
    uint16_t                                    requirements;
    int                                         (dosamp_FAR *open)(soundcard_t sc);
    int                                         (dosamp_FAR *close)(soundcard_t sc);
    int                                         (dosamp_FAR *poll)(soundcard_t sc);
    uint32_t                                    (dosamp_FAR *can_write)(soundcard_t sc); /* in bytes */
    int                                         (dosamp_FAR *clamp_if_behind)(soundcard_t sc,uint32_t ahead_in_bytes);
    int                                         (dosamp_FAR *irq_callback)(soundcard_t sc);
    unsigned int                                (dosamp_FAR *write)(soundcard_t sc,const unsigned char dosamp_FAR * buf,unsigned int len);
    unsigned char dosamp_FAR *                  (dosamp_FAR *mmap_write)(soundcard_t sc,uint32_t dosamp_FAR * const howmuch,uint32_t want);
    int                                         (dosamp_FAR *ioctl)(soundcard_t sc,unsigned int cmd,void dosamp_FAR *data,unsigned int dosamp_FAR * len,int ival);
    struct wav_state_t                          wav_state;
    struct wav_cbr_t                            cur_codec;
    union {
        struct soundcard_priv_soundblaster_t    soundblaster;
    } p;
};

/* open: allocate resources for playback or recording.
 *
 * close: free resources for playback and recording.
 *
 * poll: update internal/external state of the sound card
 *
 * can_write: return in bytes how much data can be written to the sound card without blocking
 *
 * clamp_if_behind: check play pointer vs write pointer. if the play pointer passed the write pointer (underrun) then advance write pointer
 *                  so that next written audio occurs at the play pointer plus an offset provided by the caller.
 *
 * irq_callback: call this method from your IRQ handler
 *
 * write: write audio data. copy data from caller's buffer into the sound card's playback buffers.
 *
 * mmap_write: write audio data. if supported, returns a pointer within sound card's playback buffer and size that caller MUST write to fill
 *             in buffer. advances write pointer. the byte count returned in *howmuch MUST be filled in by that many bytes.
 *
 * ioctl: general entry point for minor functions. */

/* ioctls */
#define soundcard_ioctl_silence_buffer                      0x5B00U /* fill buffer with silence */
#define soundcard_ioctl_get_irq                             0x5B10U /* return IRQ used by sound card, or -1 if not supported */
#define soundcard_ioctl_set_irq_interval                    0x5B11U /* set IRQ interval, if IRQ supported */
#define soundcard_ioctl_read_irq_counter                    0x5B12U /* read IRQ counter, if IRQ supported */
#define soundcard_ioctl_isa_dma_assign_buffer               0x5B1AU /* assign DMA buffer to sound card, if ISA DMA is supported */
#define soundcard_ioctl_isa_dma_channel                     0x5B1DU /* get DMA channel sound card will use, if ISA DMA supported, else -1 */
#define soundcard_ioctl_isa_dma_recommended_buffer_size     0x5B1EU /* specify buffer size limit, and return recomended ISA DMA buffer size, else -1 */
#define soundcard_ioctl_get_autoinit                        0x5BA1U /* get flag indicating whether sound card will use auto-init playback method */
#define soundcard_ioctl_set_autoinit                        0x5BA2U /* set flag indicating whether sound card will use auto-init playback method */
#define soundcard_ioctl_prepare_play                        0x5B40U /* prepare card for playback */
#define soundcard_ioctl_unprepare_play                      0x5B41U /* undo preparation for playback */
#define soundcard_ioctl_start_play                          0x5B42U /* start playback */
#define soundcard_ioctl_stop_play                           0x5B43U /* stop playback */
#define soundcard_ioctl_get_buffer_size                     0x5BB0U /* get playback buffer size */
#define soundcard_ioctl_get_buffer_write_position           0x5BB1U /* get write position within buffer */
#define soundcard_ioctl_get_buffer_play_position            0x5BB2U /* get play position within buffer (e.g. ISA DMA pointer) */
#define soundcard_ioctl_set_play_format                     0x5BF0U /* set play format. specify wav_cbr_t which will be modifed to supported format, or -1 if not support */

