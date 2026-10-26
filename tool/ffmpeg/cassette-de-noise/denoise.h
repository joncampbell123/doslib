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
};

class denoise_t {
public:
    ~denoise_t();

    bool                        init(const noise_params_t &p,int sample_rate,int channels);

    // first pass: the whole input, to find the level of the noise
    void                        analyze(const float *const *x,int n);
    void                        end_analysis(void);

    // second pass: the same input again, filtered; the samples out are added to the end of out[]
    void                        filter(const float *const *x,int n,std::vector<float> *out);
    void                        end_filter(std::vector<float> *out);

    // the noise level found, in dB of full scale, from -from up, at a time (seconds), or the middle of it over time
    double                      noise_db_at(int ch,double t) const;
    double                      noise_db(int ch) const;
    // how much the noise was turned down by, in dB, where there was only the noise (the middle of it over time)
    double                      reduced_db(int ch) const;

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
        std::vector<float>      gr;                 // [ring][bin]: the gain, before smoothing
        std::vector<float>      g;                  // this frame's gain, smoothed
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
    };

    void                        feed(const float *const *x,int n,bool filtering,std::vector<float> *out);
    void                        frame(bool filtering,std::vector<float> *out);
    void                        find_noise(void);
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
};

#endif
