/* derumble.cpp: turning down the low rumble of a cassette tape recording (see derumble.h).
 *
 * Each channel is cut into frames of fft samples, a quarter of a frame apart, each one Hann windowed and
 * transformed. Only the change to each frequency is transformed back: it is windowed again and the frames
 * added up (four Hann squared windows a quarter apart add up to 1.5 everywhere), and added to the input.
 * Where nothing is changed, the output is the input to the bit, whatever its sample format, and the
 * rounding of the transforms is only in the change. The output is three hops behind the input, which is
 * taken back out, so that the output lines up with the input sample for sample, and is as long. */

#include <string.h>
#include <math.h>

#include <algorithm>

extern "C" {
#include <libavutil/mem.h>
}

#include "derumble.h"

derumble_t::~derumble_t() {
    av_tx_uninit(&fwd);
    av_tx_uninit(&inv);
    av_freep(&buf);
    av_freep(&spec);
}

bool derumble_t::init(const rumble_params_t &np,int sample_rate,int nchannels) {
    p = np;
    rate = sample_rate;
    channels = nchannels;
    if (rate <= 0 || channels <= 0)
        return false;

    fft = p.fft;
    if (fft <= 0) {
        fft = 256;
        while (fft < (int)(rate * 0.085)) fft *= 2;
    }
    hop = fft / 4;

    // the low frequencies worked on: from 0 up to twice the cutoff, the rest are left alone
    bins = 1;
    while (bins < fft / 2 - 1 && ((double)bins * rate) / fft < p.cutoff * 2) bins++;

    weight.resize(bins);
    for (int j=0;j < bins;j++) {
        const double f = ((double)j * rate) / fft;
        if (f <= p.cutoff)
            weight[j] = 1;
        else if (f >= p.cutoff * 2)
            weight[j] = 0;
        else
            weight[j] = (float)(0.5 + 0.5 * cos(M_PI * log2(f / p.cutoff)));
    }

    win.resize(fft);
    for (int i=0;i < fft;i++)
        win[i] = (float)(0.5 - 0.5 * cos((2 * M_PI * i) / fft));

    av_freep(&buf);
    av_freep(&spec);
    buf = (float*)av_malloc(sizeof(float) * fft);
    spec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * (fft / 2 + 1));
    if (buf == NULL || spec == NULL)
        return false;
    ps.resize(bins);

    float scale = 1.0f;
    if (av_tx_init(&fwd,&fwd_fn,AV_TX_FLOAT_RDFT,0,fft,&scale,0) < 0)
        return false;
    scale = 1.0f / fft;
    if (av_tx_init(&inv,&inv_fn,AV_TX_FLOAT_RDFT,1,fft,&scale,0) < 0)
        return false;

    ch.resize(channels);
    for (auto &c : ch) {
        c.in.assign(fft,0);
        c.acc.assign(fft,0);
        c.S.assign(bins,0);
        c.gain.assign(bins,1);
        c.block.assign(bins,0);
    }

    // blocks of about half a second, for the rumble level over time
    block_frames = std::max(1,(int)lrint((0.5 * rate) / hop));

    smooth_a = (float)exp(-(double)hop / (0.03 * rate));
    release_a = p.release > 0 ? (float)exp(-(double)hop / (p.release * rate)) : 0.0f;
    gmin = (float)pow(10.0,-p.reduce / 20);

    fill = 0;
    frames = samples_in = samples_out = block_count = 0;
    return true;
}

void derumble_t::feed(const float *const *x,int n,bool filtering,std::vector<float> *out) {
    int at = 0;

    while (at < n) {
        const int todo = std::min(n - at,hop - fill);

        for (int c=0;c < channels;c++) {
            float *d = ch[c].in.data() + (fft - hop) + fill;
            if (x != NULL)
                memcpy(d,x[c] + at,sizeof(float) * todo);
            else
                memset(d,0,sizeof(float) * todo);
        }

        fill += todo;
        at += todo;
        if (fill == hop) {
            frame(filtering,out);
            for (auto &c : ch)
                memmove(c.in.data(),c.in.data() + hop,sizeof(float) * (fft - hop));
            fill = 0;
        }
    }
}

void derumble_t::frame(bool filtering,std::vector<float> *out) {
    // where this frame is in the blocks of the rumble level, the middle of block b being at b*B + (B-1)/2
    double pos = (frames - (block_frames - 1) * 0.5) / block_frames;
    int b0 = 0,b1 = 0;
    float bf = 0;

    if (filtering && nblocks > 0) {
        pos = std::max(0.0,std::min(pos,(double)(nblocks - 1)));
        b0 = (int)floor(pos);
        b1 = std::min(b0 + 1,nblocks - 1);
        bf = (float)(pos - b0);
    }

    for (int ci=0;ci < channels;ci++) {
        derumble_chan_t &c = ch[ci];

        for (int i=0;i < fft;i++)
            buf[i] = c.in[i] * win[i];
        fwd_fn(fwd,spec,buf,sizeof(float));

        if (!filtering) {
            for (int j=0;j < bins;j++)
                c.block[j] += (double)spec[j].re * spec[j].re + (double)spec[j].im * spec[j].im;
            continue;
        }

        // the power of each low frequency, smoothed across its neighbors and over time
        for (int j=0;j < bins;j++) {
            const int jl = j > 0 ? j - 1 : 1;
            const AVComplexFloat &l = spec[jl],&m = spec[j],&h = spec[j + 1];

            ps[j] = 0.25f * (l.re * l.re + l.im * l.im) + 0.5f * (m.re * m.re + m.im * m.im) + 0.25f * (h.re * h.re + h.im * h.im);
            c.S[j] = smooth_a * c.S[j] + (1 - smooth_a) * ps[j];
        }

        const float *n0 = nblocks > 0 ? c.noise.data() + (size_t)b0 * bins : NULL;
        const float *n1 = nblocks > 0 ? c.noise.data() + (size_t)b1 * bins : NULL;

        double e_in = 0,e_out = 0;
        bool changed = false;

        for (int j=0;j < bins;j++) {
            const float noise = n0 != NULL ? (float)p.over * (n0[j] + bf * (n1[j] - n0[j])) : 0.0f;
            const double f = ((double)j * rate) / fft;
            float g;

            // turned down by as much of it as is the rumble
            if (f < p.highpass)
                g = 0;
            else if (c.S[j] > noise)
                g = sqrtf(1 - noise / c.S[j]);
            else
                g = noise > 0 ? 0 : 1;

            g = std::max(g,gmin);
            // up at once for a sound, back down slowly after it, which keeps the rumble left from warbling
            if (g < c.gain[j])
                g = release_a * c.gain[j] + (1 - release_a) * g;
            c.gain[j] = g;

            const float gw = 1 - weight[j] * (1 - g);
            const double pw = (double)spec[j].re * spec[j].re + (double)spec[j].im * spec[j].im;

            if (f < p.cutoff && f >= p.highpass) {
                e_in += pw;
                e_out += pw * gw * gw;
            }

            // the change to this frequency
            spec[j].re *= gw - 1;
            spec[j].im *= gw - 1;
            if (gw != 1)
                changed = true;
        }
        for (int j=bins;j <= fft / 2;j++)
            spec[j].re = spec[j].im = 0;

        // not counting digital silence
        if (e_in > 1e-6)
            c.reduced.push_back(e_out > 0 ? (float)(10 * log10(e_in / e_out)) : 300.0f);

        if (!changed)
            continue;

        inv_fn(inv,buf,spec,sizeof(AVComplexFloat));
        for (int i=0;i < fft;i++)
            c.acc[i] += buf[i] * win[i] * (1.0f / 1.5f);
    }

    if (!filtering) {
        if (++block_count == block_frames) {
            for (auto &c : ch) {
                for (int j=0;j < bins;j++) {
                    c.blocks.push_back((float)(c.block[j] / block_count));
                    c.block[j] = 0;
                }
            }
            block_count = 0;
            nblocks++;
        }
    }
    else {
        // the first hop of acc is complete, the change to the first hop of this frame's input;
        // the first three frames are before the start
        if (frames >= 3) {
            const int64_t n = std::min((int64_t)hop,samples_in - samples_out);
            for (int ci=0;ci < channels;ci++) {
                const derumble_chan_t &c = ch[ci];
                for (int64_t i=0;i < n;i++)
                    out[ci].push_back(c.in[i] + c.acc[i]);
            }
            samples_out += n;
        }
        for (auto &c : ch) {
            memmove(c.acc.data(),c.acc.data() + hop,sizeof(float) * (fft - hop));
            memset(c.acc.data() + (fft - hop),0,sizeof(float) * hop);
        }
    }

    frames++;
}

void derumble_t::analyze(const float *const *x,int n) {
    samples_in += n;
    feed(x,n,false,NULL);
}

void derumble_t::end_analysis(void) {
    // the frames that the filter will have at the end, over the last of it
    feed(NULL,fft - hop + (hop - fill) % hop,false,NULL);
    if (block_count > 0) {
        for (auto &c : ch) {
            for (int j=0;j < bins;j++) {
                c.blocks.push_back((float)(c.block[j] / block_count));
                c.block[j] = 0;
            }
        }
        block_count = 0;
        nblocks++;
    }

    find_noise();

    for (auto &c : ch) {
        std::fill(c.in.begin(),c.in.end(),0.0f);
        std::vector<float>().swap(c.blocks);
    }
    fill = 0;
    frames = samples_in = samples_out = 0;
}

void derumble_t::find_noise(void) {
    const int W = std::max(1,(int)lrint((p.window * 0.5 * rate) / ((double)hop * block_frames)));
    std::vector<float> v;
    std::vector<char> silent(nblocks);

    for (auto &c : ch) {
        c.noise.assign((size_t)nblocks * bins,0);

        // blocks of digital silence (before and after the recording) say nothing of the rumble
        for (int b=0;b < nblocks;b++) {
            double s = 0;
            for (int j=0;j < bins;j++) s += c.blocks[(size_t)b * bins + j];
            silent[b] = s < 1e-9;
        }

        for (int b=0;b < nblocks;b++) {
            const int lo = std::max(0,b - W),hi = std::min(nblocks - 1,b + W);

            for (int j=0;j < bins;j++) {
                v.clear();
                for (int k=lo;k <= hi;k++) {
                    if (!silent[k])
                        v.push_back(c.blocks[(size_t)k * bins + j]);
                }
                if (v.empty())
                    continue;

                const size_t at = (size_t)((p.percentile / 100) * (v.size() - 1) + 0.5);
                std::nth_element(v.begin(),v.begin() + at,v.end());
                c.noise[(size_t)b * bins + j] = v[at];
            }
        }
    }
}

void derumble_t::filter(const float *const *x,int n,std::vector<float> *out) {
    samples_in += n;
    feed(x,n,true,out);
}

void derumble_t::end_filter(std::vector<float> *out) {
    while (samples_out < samples_in)
        feed(NULL,hop,true,out);
}

// power of the windowed transform to mean square of the samples: sum of both sides of |X|^2 is
// fft * sum((x*w)^2), and the mean of w^2 is 3/8
static double power_to_db(double pw,int fft) {
    const double ms = (2 * pw) / ((double)fft * fft * 0.375);
    return 10 * log10(ms + 1e-30);
}

double derumble_t::rumble_db_at(int ci,double t) const {
    if (nblocks <= 0)
        return -300;

    const int b = std::max(0,std::min(nblocks - 1,(int)((t * rate) / ((double)hop * block_frames))));
    double s = 0;

    for (int j=0;j < bins;j++) {
        const double f = ((double)j * rate) / fft;
        if (f >= p.highpass && f < p.cutoff)
            s += ch[ci].noise[(size_t)b * bins + j];
    }

    return power_to_db(s,fft);
}

double derumble_t::rumble_db(int ci) const {
    std::vector<double> v;

    for (int b=0;b < nblocks;b++)
        v.push_back(rumble_db_at(ci,((double)b * hop * block_frames) / rate));
    if (v.empty())
        return -300;

    std::nth_element(v.begin(),v.begin() + v.size() / 2,v.end());
    return v[v.size() / 2];
}

double derumble_t::reduced_db(int ci) const {
    std::vector<float> v = ch[ci].reduced;

    if (v.empty())
        return 0;

    std::nth_element(v.begin(),v.begin() + v.size() / 2,v.end());
    return v[v.size() / 2];
}
