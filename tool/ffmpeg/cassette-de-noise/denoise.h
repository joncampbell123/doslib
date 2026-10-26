/* denoise.h: turning down the hiss and noise of a cassette tape recording, without the burbling.
 *
 * The sound of each channel is taken apart into its frequencies (a short time Fourier transform), and each
 * frequency of each moment is turned down where it is no more than the noise. The noise is the hiss of the
 * tape, and the noise of the recorder and the room: about the same all the time, the same at each frequency
 * from moment to moment (it can change from one recording on a tape to the next). Its level at each frequency
 * is found from the whole recording first: the quiet parts of each minute or so, where there is only the
 * noise (a low percentile).
 *
 * Most noise reduction decides each frequency of each moment by itself: whether it is louder than the noise.
 * Noise alone is louder than its own level at some of them, by chance, a different few each moment; each of
 * those is let through for a moment, and the noise that is left is little tones that come and go at random,
 * which is heard as burbling or bubbling ("musical noise"). Smoothing the decisions over time only makes the
 * tones longer.
 *
 * Here each frequency is decided from the power of the tiles of frequencies and moments around it: a single
 * frequency over 3 frames, 1/12 octave over 3 frames, 1/6 octave over 5 frames, and 1/3 octave over 7 frames
 * (about 10ms a frame). The power of noise alone, over a tile, is very nearly its level, closer the bigger the
 * tile; how near is known (a Gamma distribution, of as many independent values as there are in the tile), so
 * each size of tile has a threshold that noise alone goes over only about once in some thousands or millions
 * of times (-threshold, in standard deviations), plus a margin for noise that is not quite as steady as all
 * that (-margin). A frequency is let through by as much as the biggest of its tiles is over the threshold:
 * a sound that is a single tone gets through by its own frequency, a sound that is spread out, such as a
 * voice or a cymbal, by the bigger tiles, which find it even when it is under the noise at each frequency.
 * Noise alone is then turned down by the same amount (-reduce) everywhere and all the time, which leaves it
 * sounding the same, only quieter, with nothing coming and going in it.
 *
 * With -iterate, the input is gone through more times, to tell the sound from the noise better:
 *
 * - The level of the noise: where the sound goes on all the time, such as music with no quiet parts, the quiet
 *   part of each minute is the music, not the noise, and the noise is taken to be louder than it is. Each time
 *   through, each frequency of each moment where nothing was let through, and that is not louder than the noise
 *   by more than chance, is taken as noise alone, and the level of the noise is found again from those (their
 *   mean, made up for the ones left out by chance): each time the noise is found quieter, more of the quiet
 *   sound is let through, and the noise is found from what is left. Where there are quiet parts, it is found
 *   as it was, closer.
 *
 * - The gain of each frequency: once a tile is let through, all its frequencies are, and the noise between the
 *   harmonics of a voice or an instrument with them. With -iterate, the sound let through is taken as the
 *   sound, and each frequency is let through by as much of it as that is (a Wiener filter, of the power of the
 *   sound let through less the noise let through with it, over the 3 frequencies and 3 frames around it), up to
 *   as much as the tiles let through: up to twice, the second from the first.
 *
 * With -noise, the level of the noise is not found, but taken from a part of the input that is noise alone (such
 * as blank tape before or after the recording, from the same recorder and tape): the mean power of each frequency
 * there (smoothed over 1/12 octave), the same all through. Where the sound goes on with no quiet parts, the quiet
 * part of each minute is not the noise but the quiet sound (the room, a distant voice), which is then taken for
 * noise and turned down with it; a part with only the noise takes only the noise out. -noiseadjust turns the level
 * of the noise, found or taken, up or down by so many dB.
 *
 * With -attacks, the frames are not all as long: a long transform is only as sharp in time as its frames, and
 * where a sound starts sharply (an attack: a drum, a consonant, a click), what is done to the frame is done to
 * all of it, so the noise is let through for up to a frame before the attack, which dulls it. A second, short
 * transform (about 5ms, 256 samples at 48000Hz) finds the attacks, as a rise of more than 6dB within 20ms of the power of an
 * octave band from 300Hz up, over the noise by more than chance; around each, from 40ms before it to 25ms after,
 * the shorter transform is used for that band, the longer one everywhere else, faded from one to the other
 * over 10ms. Each takes its part of the change (their parts adding up to all of it), so noise alone is still
 * turned down evenly.
 *
 * Where nothing is turned down, the output is the input to the bit (only the change is transformed back). */

#ifndef DENOISE_H
#define DENOISE_H

#include <stdint.h>

#include <vector>

extern "C" {
#include <libavutil/tx.h>
}

struct noise_params_t {
    double                      reduce = 12;        // dB: how much the noise is turned down
    double                      from = 0;           // Hz: only from this frequency up (less and less down to half of it), 0 for all
    double                      threshold = 3.7;    // standard deviations of the noise over a tile that is taken as more than noise
    double                      margin = 1;         // dB: more than that, for noise that is not quite steady
    double                      window = 60;        // seconds: the noise level is found from this much around each moment
    double                      percentile = 10;    // the quiet part of each window taken as the noise level
    int                         fft = 0;            // samples to a transform, a power of 2, or 0 for about 43ms
    bool                        link = false;       // the same gain to all the channels, decided from all of them
    int                         iterate = 0;        // times more through the input, to find the noise again; and the gain made finer up to twice
    double                      adjust = 0;         // dB: the level of the noise turned up or down by this much
    bool                        change_only = false;// put out only the change, not the input with it (-attacks, to add the two up)
    bool                        attacks = false;    // this is the shorter transform for -attacks: keep the power of the octave bands
};

// with -attacks: where the sounds start sharply, of each channel and octave band, and how much the shorter
// transform is used there, from 0 to 1
struct attack_map_t {
    double                      before = 0.040;     // seconds before an attack that the shorter transform is used
    double                      after = 0.025;      // and after
    double                      ramp = 0.010;       // faded in and out over this
    double                      rise_db = 6;        // a rise of this much
    double                      lookback = 0.020;   // within this
    int                         rate = 0;
    std::vector<double>         edges;              // Hz: of the octave bands, from 300Hz
    std::vector<std::vector<std::vector<double>>> onsets;   // [channel][band]: the time of each attack, seconds, in order

    float                       weight(int ch,int band,double t) const;
    int                         band_of(double hz) const;
    size_t                      count(void) const;
};

class denoise_t {
public:
    ~denoise_t();

    bool                        init(const noise_params_t &p,int sample_rate,int channels);

    // first pass: the whole input, to find the level of the noise
    void                        analyze(const float *const *x,int n);
    void                        end_analysis(void);

    // with -noise, before the first pass: each part of the input that is noise alone, then the end of it; the
    // level of the noise is then the mean of them, instead of being found
    void                        profile(const float *const *x,int n);
    void                        end_profile_part(void);
    bool                        has_profile(void) const;

    // with -iterate, after the first pass and before the last, each time: the whole input again, to find the
    // level of the noise again from what is not let through
    void                        reestimate(const float *const *x,int n);
    void                        end_reestimate(void);

    // second pass: the same input again, filtered; the samples out are added to the end of out[]
    void                        filter(const float *const *x,int n,std::vector<float> *out);
    void                        end_filter(std::vector<float> *out);

    // the noise level found, in dB of full scale, from -from up, at a time (seconds), or the middle of it over time
    double                      noise_db_at(int ch,double t) const;
    double                      noise_db(int ch) const;
    // how much the noise was turned down by, in dB, where there was only the noise (the middle of it over time)
    double                      reduced_db(int ch) const;

    // -attacks: the shorter transform finds the attacks, from the power of its octave bands and the noise found;
    // then each transform gets its part of the change from them
    void                        find_attacks(attack_map_t &m) const;
    void                        use_attacks(const attack_map_t *m,bool shorter);

    int                         fft = 0;
    int                         hop = 0;
    int                         bins = 0;
    int                         nblocks = 0;
private:
    struct scale_t {
        int                     ht = 0;             // frames either side
        std::vector<int>        lo,hi;              // of each frequency: the frequencies of its tile
        std::vector<float>      lambda;             // of each frequency: the threshold, over the noise level
    };

    // a channel, as the gain is decided: one for each channel, or one for all of them with -link
    struct decide_t {
        std::vector<float>      P,N;                // power and noise of this frame
        std::vector<double>     cum;                // prefix sums, for the tiles
        std::vector<float>      Pf,Nf;              // [scale][ring][bin]: power and noise over the frequencies of the tile
        std::vector<double>     Ps,Ns;              // [scale][bin]: and over the frames of the tile
        std::vector<float>      Pr,Nr;              // [ring][bin]: power and noise of the last frames
        std::vector<float>      gr;                 // [ring][bin]: the gain, before smoothing
        std::vector<float>      G;                  // [step][ring][bin]: the gain smoothed, then made finer (-iterate)
        std::vector<float>      h;                  // [step][ring][bin]: the gain made finer, before smoothing
        std::vector<float>      g;                  // the gain put on the frame now
    };

    struct chan_t {
        float*                  buf = NULL;         // the windowed frame (av_malloc, aligned for the transform)
        AVComplexFloat*         spec = NULL;        // its frequencies
        AVTXContext*            fwd = NULL;         // each channel its own, so that the channels can be done at once
        AVTXContext*            inv = NULL;
        av_tx_fn                fwd_fn = NULL;
        av_tx_fn                inv_fn = NULL;
        std::vector<float>      in;                 // the last fft samples in
        std::vector<float>      acc;                // the sum of the changes out, the first hop of it complete
        std::vector<float>      dly;                // the input, waiting to be put out with the change
        size_t                  dly_at = 0;
        std::vector<AVComplexFloat> X;              // [ring][bin]: the transforms of the last frames
        std::vector<double>     block;              // analysis: sum of the smoothed power, this block
        std::vector<float>      blocks;             // analysis: mean of it, each block, then the noise level
        std::vector<float>      reduced;            // dB the noise was turned down by, each frame where it was all at the floor
        std::vector<float>      esum,ecnt;          // finding the noise again: power and count of the noise alone, each block
        std::vector<uint16_t>   bandpow;            // -attacks: [frame][band]: power of the octave bands, in 1/256 dB over -200dB
        std::vector<double>     psum;               // -noise: the power of each frequency, summed over the parts given
        int64_t                 pcount = 0;         // and the frames of it
    };

    enum pass_t { PROFILE, ANALYZE, REESTIMATE, FILTER };

    void                        feed(const float *const *x,int n,pass_t pass,std::vector<float> *out);
    void                        frame(pass_t pass,std::vector<float> *out);
    void                        decide(decide_t &D,int di,int64_t f,pass_t pass);
    void                        end_pass(pass_t pass);
    void                        reset(void);
    void                        find_noise(void);
    void                        find_noise_again(void);
    void                        noise_from_profile(void);
    void                        box(const std::vector<float> &v,std::vector<double> &cum,const scale_t &s,float *out);

    noise_params_t              p;
    int                         rate = 0;
    int                         channels = 0;
    std::vector<chan_t>         ch;
    std::vector<decide_t>       dec;
    std::vector<scale_t>        scales;
    scale_t                     nscale;             // the smoothing of the noise level over frequency
    std::vector<float>          nq;                 // of each frequency: the percentile of noise alone, over its mean
    std::vector<float>          win;
    std::vector<float>          weight;             // of each frequency: 1 where it is turned down, 0 where not (-from)
    int                         block_frames = 0;
    int                         block_count = 0;
    int                         fill = 0;
    int64_t                     frames = 0;
    int64_t                     samples_in = 0;
    int64_t                     samples_out = 0;
    float                       gmin = 1;
    float                       nadj = 1;           // -noiseadjust: the level of the noise is used times this
    int                         refine = 0;         // times the gain is made finer
    float                       clip = 0;           // noise alone over 3x3 frequencies and frames is under this, 99 times in 100
    float                       clip_mean = 1;      // and its mean, under that, over its mean
    // -attacks
    std::vector<double>         aedges;             // Hz: octave bands, from 300Hz
    std::vector<int>            aband;              // of each frequency: its octave band, -1 below them
    const attack_map_t*         amap = NULL;
    bool                        ashort = false;     // this is the shorter transform
    void                        attack_weights(int di,int64_t d,std::vector<float> &w) const;
};

#endif
