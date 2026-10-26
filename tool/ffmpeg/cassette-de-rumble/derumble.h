/* derumble.h: turning down the low rumble of a cassette tape recording.
 *
 * The rumble of a home cassette recording is the motor, the tape rubbing in the shell, or the recorder
 * moving in a pocket or a bag, picked up by the microphone. It is a low noise, from the bottom up to
 * a few hundred Hz, that goes on under everything at about the same level, heard most where it is
 * quiet. A high pass filter would take the bass of the voices and the music with it.
 *
 * Instead the sound of each channel is taken apart into its frequencies (a short time Fourier transform),
 * and only the low ones are changed. The level of the rumble at each low frequency is found from the
 * whole recording first: the quiet parts of each few seconds, where there is only the rumble (a low
 * percentile). Then each frequency of each moment is turned down by as much of it as is the rumble,
 * so that what is louder than the rumble (a voice, a bass note) stays, and where there is only the
 * rumble, it is turned down by the most.
 *
 * Above the cutoff nothing is changed: those frequencies come out as they went in. */

#ifndef DERUMBLE_H
#define DERUMBLE_H

#include <stdint.h>

#include <vector>

extern "C" {
#include <libavutil/tx.h>
}

struct rumble_params_t {
    double                      cutoff = 200;       // Hz: the rumble is taken out below this, less and less up to twice this
    double                      highpass = 20;      // Hz: below this, all of it is rumble, turned down by the most
    double                      reduce = 24;        // dB: the most the rumble is turned down
    double                      over = 3;           // how much more than the level found is taken as rumble
    double                      window = 60;        // seconds: the rumble level is found from this much around each moment
    double                      percentile = 10;    // the quiet part of each window taken as the rumble level
    double                      release = 0.08;     // seconds: how slowly a frequency is turned back down after a sound
    int                         fft = 0;            // samples to a transform, a power of 2, or 0 for about 85ms
};

// one channel's transform, in and out
struct derumble_chan_t {
    std::vector<float>          in;                 // the last fft samples in
    std::vector<float>          acc;                // the sum of the frames out, the first hop of it complete
    std::vector<float>          S;                  // smoothed power of each low frequency
    std::vector<float>          gain;               // the gain of each low frequency, last frame
    std::vector<double>         block;              // analysis: sum of power of each low frequency, this block
    std::vector<float>          blocks;             // analysis: mean power of each low frequency, each block
    std::vector<float>          noise;              // the rumble level of each low frequency, each block
    std::vector<float>          reduced;            // dB the band below the cutoff was turned down by, each frame, for the report
};

class derumble_t {
public:
    ~derumble_t();

    bool                        init(const rumble_params_t &p,int sample_rate,int channels);

    // first pass: the whole input, to find the level of the rumble
    void                        analyze(const float *const *x,int n);
    void                        end_analysis(void);

    // second pass: the same input again, filtered; the samples out are added to the end of out[]
    void                        filter(const float *const *x,int n,std::vector<float> *out);
    void                        end_filter(std::vector<float> *out);  // the rest of it, as many out as went in

    // the rumble level found, in dB of full scale, over the band up to the cutoff, of each channel
    double                      rumble_db(int ch) const;
    // how much the band below the cutoff was turned down by, in dB, of each channel: the middle of it over time
    double                      reduced_db(int ch) const;
    // the rumble level found at a time (seconds), dB, of a channel
    double                      rumble_db_at(int ch,double t) const;

    int                         fft = 0;
    int                         hop = 0;
    int                         bins = 0;           // the low frequencies worked on, from 0 up to twice the cutoff
    int                         block_frames = 0;   // frames to a block of the rumble level
    int                         nblocks = 0;
private:
    void                        feed(const float *const *x,int n,bool filtering,std::vector<float> *out);
    void                        frame(bool filtering,std::vector<float> *out);
    void                        find_noise(void);

    rumble_params_t             p;
    int                         rate = 0;
    int                         channels = 0;
    std::vector<derumble_chan_t> ch;
    std::vector<float>          win;                // Hann window
    std::vector<float>          weight;             // of each low frequency, 1 below the cutoff to 0 at twice it
    float*                      buf = NULL;         // the windowed frame (av_malloc, aligned for the transform)
    AVComplexFloat*             spec = NULL;        // its frequencies
    std::vector<float>          ps;                 // its power, smoothed across the frequencies
    AVTXContext*                fwd = NULL;
    AVTXContext*                inv = NULL;
    av_tx_fn                    fwd_fn = NULL;
    av_tx_fn                    inv_fn = NULL;
    int                         fill = 0;           // samples into the hop now being filled
    int64_t                     frames = 0;         // frames done, this pass
    int64_t                     samples_in = 0;     // this pass
    int64_t                     samples_out = 0;    // this pass, not counting the delay at the start
    int64_t                     block_count = 0;    // frames summed into the block now
    float                       smooth_a = 0;       // power smoothing over time, per frame
    float                       release_a = 0;      // gain release, per frame
    float                       gmin = 1;
};

#endif
