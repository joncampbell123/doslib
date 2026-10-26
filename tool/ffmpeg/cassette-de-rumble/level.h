/* level.h: taking out the small rapid changes of level that go with the rumble.
 *
 * Where the recorder itself shakes, the microphone picks it up as rumble, and the tape is pushed against
 * and away from the head, which turns the high frequencies up and down with it (the further the tape is
 * from the head, the more of the high frequencies are lost: the loss in dB goes up with the frequency).
 * So the level of each band of frequencies goes up and down with the waveform of the rumble, tens of
 * times a second, which is heard as a roughness of the voices and the music.
 *
 * How much the level of each band goes with the rumble is not known beforehand: it is found from the
 * recording, the first time through, for each part of it. The level of each band (in dB, about every
 * 1.3ms) is compared with the waveform of the rumble of each channel, at each rate of change from 5 to
 * 100 times a second, and from that the change in level that the rumble makes is worked out (a linear
 * filter from the rumble waveforms to the level of each band, which is how much of the level the rumble
 * accounts for: none of it where the level does not go with the rumble, such as where the rumble is not
 * the recorder shaking). The second time through, that change is predicted from the rumble and taken
 * back out, frequency by frequency, which leaves the level of everything else as it is. */

#ifndef LEVEL_H
#define LEVEL_H

#include <stdint.h>

#include <map>
#include <vector>

extern "C" {
#include <libavutil/tx.h>
}

struct level_params_t {
    double                      window = 120;       // seconds: how the level goes with the rumble is found from this much around each part
    double                      max = 4;            // dB: the most the level is changed by
};

// a 2nd order filter section
struct biquad_t {
    double                      b0 = 1,b1 = 0,b2 = 0,a1 = 0,a2 = 0;
    double                      z1 = 0,z2 = 0;

    void                        lowpass(double f,double q,int rate);
    void                        highpass(double f,double q,int rate);
    double                      run(double x) {
        const double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct level_chan_t {
    std::vector<float>          in;                 // the last fft samples in
    std::vector<float>          acc;                // the sum of the changes out, the first hop of it complete
    biquad_t                    bp[3];              // the rumble band, 20 to 250Hz
    std::vector<float>          r;                  // the rumble waveform, one to a frame, the whole input
    std::vector<float>          ering;              // analysis: the level of each band, the last seglen frames
    std::vector<float>          h;                  // the filter from the rumble to the level, each block, band, rumble channel
    std::vector<char>           linked;             // each block: whether there is a filter at all
    std::vector<float>          explained;          // each block: how much of the change of level the rumble accounts for, 0 to 1
    double                      msum = 0;           // the change made, for the report
    double                      mmax = 0;
    int64_t                     mcount = 0;
};

class leveler_t {
public:
    ~leveler_t();

    bool                        init(const level_params_t &p,int sample_rate,int channels);

    // first pass: the whole input, to find how the level goes with the rumble
    void                        analyze(const float *const *x,int n);
    void                        end_analysis(void);

    // second pass: the same input again (or as the de-rumble filter put it out, which is the same above
    // the rumble), the level put right; the samples out are added to the end of out[]
    void                        filter(const float *const *x,int n,std::vector<float> *out);
    void                        end_filter(std::vector<float> *out);

    // for the report: the seconds where the level was found to go with the rumble, of each channel
    double                      linked_seconds(int ch) const;
    // how much of the change of level (5 to 100 times a second) the rumble accounts for, 0 to 1, at a time
    double                      explained_at(int ch,double t) const;
    double                      change_rms_db(int ch) const;
    double                      change_max_db(int ch) const;

    int                         fft = 0;
    int                         hop = 0;
    int                         nbands = 0;
private:
    void                        feed(const float *const *x,int n,bool filtering,std::vector<float> *out);
    void                        frame(bool filtering,std::vector<float> *out);
    void                        segment(void);
    void                        find_filters(void);
    const std::vector<float>&   prediction(int block);

    level_params_t              p;
    int                         rate = 0;
    int                         channels = 0;
    std::vector<level_chan_t>   ch;
    std::vector<float>          win;                // Hann window, fft
    std::vector<double>         edges;              // of the bands, Hz
    std::vector<int>            bin_band;           // of each frequency: the band below it, -1 for none
    std::vector<float>          bin_frac;           // and how far it is to the next, in log frequency
    float*                      buf = NULL;
    AVComplexFloat*             spec = NULL;
    AVTXContext*                fwd = NULL;
    AVTXContext*                inv = NULL;
    av_tx_fn                    fwd_fn = NULL;
    av_tx_fn                    inv_fn = NULL;

    // comparing the level with the rumble: segments of seglen frames, half a segment apart
    int                         seglen = 0;
    int                         kmin = 0,kmax = 0;  // the rates of change used, 5 to 100 times a second
    std::vector<float>          segwin;
    float*                      sbuf = NULL;
    AVComplexFloat*             sspec = NULL;
    AVTXContext*                sfwd = NULL;
    AVTXContext*                sinv = NULL;
    av_tx_fn                    sfwd_fn = NULL;
    av_tx_fn                    sinv_fn = NULL;
    // each block, summed over its segments: level by rumble, rumble by rumble, level by level
    struct block_sums_t {
        std::vector<double>     lr;                 // [ch][band][rumble ch][k] complex, as re,im
        std::vector<double>     rr;                 // [rumble ch][rumble ch][k] complex
        std::vector<double>     ll;                 // [ch][band][k]
        int                     segs = 0;           // segments summed
    };
    std::vector<block_sums_t>   sums;
    int                         block_frames = 0;   // frames to a block
    int                         nblocks = 0;

    // the prediction of the change of level of each block, over that block and half of each next to it
    int                         pfft = 0;
    float*                      pbuf = NULL;
    AVComplexFloat*             pspec = NULL;
    AVComplexFloat*             rspec = NULL;       // the rumble of each channel, transformed
    int                         rstride = 0;        // complex values from one channel's to the next
    AVComplexFloat*             macc = NULL;        // the change of level of a band, transformed
    AVTXContext*                pfwd = NULL;
    AVTXContext*                pinv = NULL;
    av_tx_fn                    pfwd_fn = NULL;
    av_tx_fn                    pinv_fn = NULL;
    std::map<int,std::vector<float>> predicted;     // block: [ch][band][2 * block_frames], dB
    std::vector<float>          m;                  // the change this frame, each band
    std::vector<float>          mbin;               // the change this frame, each frequency

    int                         fill = 0;
    int64_t                     frames = 0;
    int64_t                     samples_in = 0;
    int64_t                     samples_out = 0;
    int64_t                     sample_at = 0;      // analysis: samples through the rumble band filter
    int64_t                     total_frames = 0;   // analysis: frames of the whole input
};

#endif
