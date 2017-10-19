
#define MAX_REBASE                  32

/* the reason I added "written/played" accounting is to make it possible
 * to robustly follow playback and act on events relative to playback.
 * such as, knowing that at such and such playback time, we looped the
 * WAV file back or switched to a new one. */
struct audio_playback_rebase_t {
    uint64_t                                event_at;       /* playback time byte count */
    unsigned long                           wav_position;   /* starting WAV position to count from using playback time */
};

extern struct audio_playback_rebase_t       wav_rebase_events[MAX_REBASE];
extern unsigned char                        wav_rebase_read,wav_rebase_write;

void wav_rebase_clear(void);
void rebase_flush_old(unsigned long before);
struct audio_playback_rebase_t *rebase_find(unsigned long event);
struct audio_playback_rebase_t *rebase_add(void);

