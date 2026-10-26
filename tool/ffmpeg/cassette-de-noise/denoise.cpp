/* denoise.cpp: turning down the hiss and noise of a cassette tape recording (see denoise.h).
 *
 * Each channel is cut into frames of fft samples, a quarter of a frame apart, each one Hann windowed and
 * transformed. The gain of a frame is decided from the frames up to three after it (the biggest tiles),
 * and smoothed with the frames either side of it, so the frames are put out four frames after they come in.
 * Only the change to each frequency is transformed back: it is windowed again and the frames added up (four
 * Hann squared windows a quarter apart add up to 1.5 everywhere), and added to the input, which is kept back
 * to line up with it. The output lines up with the input sample for sample, and is as long.
 *
 * The channels are done at once, with OpenMP, each with its own transforms. */

#include <string.h>
#include <math.h>

#include <algorithm>

extern "C" {
#include <libavutil/mem.h>
}

#include "denoise.h"

static const int R = 8;                             // frames kept, more than the 8 from f-7 to f that are used

// the tiles: octaves wide (0 for one frequency), frames either side
static const struct { double octaves; int ht; } tile_sizes[] = {
    { 0,      1 },
    { 1.0/12, 1 },
    { 1.0/6,  2 },
    { 1.0/3,  3 },
};

// noise alone, over a tile of k independent values, is about Gamma(k) over k, mean 1: the value it is under
// with a probability whose standard normal deviate is z, by the Wilson-Hilferty cube root normal approximation
static double gamma_quantile(double k,double z) {
    const double v = 1 / (9 * k);
    return pow(std::max(0.0,1 - v + z * sqrt(v)),3);
}

// the standard normal deviate of a probability
static double normal_deviate(double prob) {
    double lo = -10,hi = 10;
    for (int i=0;i < 100;i++) {
        const double mid = (lo + hi) / 2;
        if (0.5 * erfc(-mid / M_SQRT2) < prob) lo = mid;
        else hi = mid;
    }
    return (lo + hi) / 2;
}

denoise_t::~denoise_t() {
    for (auto &c : ch) {
        av_tx_uninit(&c.fwd);
        av_tx_uninit(&c.inv);
        av_freep(&c.buf);
        av_freep(&c.spec);
    }
}

bool denoise_t::init(const noise_params_t &np,int sample_rate,int nchannels) {
    p = np;
    rate = sample_rate;
    channels = nchannels;
    if (rate <= 0 || channels <= 0)
        return false;

    fft = p.fft;
    if (fft <= 0) {
        fft = 256;
        while (fft < (int)(rate * 0.042)) fft *= 2;
    }
    hop = fft / 4;
    bins = fft / 2 + 1;
    const double binhz = (double)rate / fft;

    // a tile of frequencies around each one, as wide as so much of an octave; and how many independent
    // values there are in it (neighboring frequencies of a Hann windowed transform go together by about
    // 1.5, and frames a quarter of a frame apart by about 2)
    auto tile = [&](scale_t &s,double octaves,int minhw) {
        s.lo.resize(bins);
        s.hi.resize(bins);
        for (int j=0;j < bins;j++) {
            const int hw = std::max(minhw,(int)lrint((j * binhz * octaves) / (2 * binhz)));
            s.lo[j] = std::max(0,j - hw);
            s.hi[j] = std::min(bins - 1,j + hw);
        }
    };

    scales.clear();
    for (const auto &ts : tile_sizes) {
        scale_t s;
        s.ht = ts.ht;
        tile(s,ts.octaves,ts.octaves > 0 ? 1 : 0);
        s.lambda.resize(bins);
        for (int j=0;j < bins;j++) {
            const double k = std::max(1.0,(s.hi[j] - s.lo[j] + 1) / 1.5) * std::max(1.0,(2 * s.ht + 1) / 2.0);
            s.lambda[j] = (float)(gamma_quantile(k,p.threshold) * pow(10.0,p.margin / 10));
        }
        scales.push_back(s);
    }

    // blocks of about half a second, for the noise level over time
    block_frames = std::max(1,(int)lrint((0.5 * rate) / hop));

    // the noise level is smoothed over 1/12 octave, at least 5 frequencies; the percentile of the blocks of noise
    // alone is under its mean by a known amount, which is taken back out
    tile(nscale,1.0 / 12,2);
    nq.resize(bins);
    {
        const double z = normal_deviate(p.percentile / 100);
        for (int j=0;j < bins;j++) {
            const double k = ((nscale.hi[j] - nscale.lo[j] + 1) / 1.5) * (block_frames / 2.0);
            nq[j] = (float)std::max(0.05,gamma_quantile(k,z));
        }
    }

    weight.resize(bins);
    for (int j=0;j < bins;j++) {
        const double f = j * binhz;
        if (p.from <= 0 || f >= p.from)
            weight[j] = 1;
        else if (f <= p.from / 2)
            weight[j] = 0;
        else
            weight[j] = (float)(0.5 - 0.5 * cos(M_PI * log2(f / (p.from / 2))));
    }

    win.resize(fft);
    for (int i=0;i < fft;i++)
        win[i] = (float)(0.5 - 0.5 * cos((2 * M_PI * i) / fft));

    ch.resize(channels);
    for (auto &c : ch) {
        c.buf = (float*)av_malloc(sizeof(float) * fft);
        c.spec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * bins);
        if (c.buf == NULL || c.spec == NULL)
            return false;

        float scale = 1.0f;
        if (av_tx_init(&c.fwd,&c.fwd_fn,AV_TX_FLOAT_RDFT,0,fft,&scale,0) < 0)
            return false;
        scale = 1.0f / fft;
        if (av_tx_init(&c.inv,&c.inv_fn,AV_TX_FLOAT_RDFT,1,fft,&scale,0) < 0)
            return false;

        c.in.assign(fft,0);
        c.acc.assign(fft,0);
        c.X.assign((size_t)R * bins,AVComplexFloat{0,0});
        c.block.assign(bins,0);
    }

    gmin = (float)pow(10.0,-p.reduce / 20);
    fill = 0;
    frames = samples_in = samples_out = 0;
    block_count = 0;
    nblocks = 0;
    return true;
}

void denoise_t::feed(const float *const *x,int n,bool filtering,std::vector<float> *out) {
    int at = 0;

    while (at < n) {
        const int todo = std::min(n - at,hop - fill);

        for (int c=0;c < channels;c++) {
            float *d = ch[c].in.data() + (fft - hop) + fill;
            if (x != NULL)
                memcpy(d,x[c] + at,sizeof(float) * todo);
            else
                memset(d,0,sizeof(float) * todo);
            if (filtering)
                ch[c].dly.insert(ch[c].dly.end(),d,d + todo);
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

// v over the frequencies of each one's tile, the mean
void denoise_t::box(const std::vector<float> &v,std::vector<double> &cum,const scale_t &s,float *o) {
    cum.resize(bins + 1);
    cum[0] = 0;
    for (int j=0;j < bins;j++)
        cum[j + 1] = cum[j] + v[j];
    for (int j=0;j < bins;j++)
        o[j] = (float)((cum[s.hi[j] + 1] - cum[s.lo[j]]) / (s.hi[j] - s.lo[j] + 1));
}

void denoise_t::frame(bool filtering,std::vector<float> *out) {
    const int64_t f = frames;
    const int slot = (int)(f % R);

    #pragma omp parallel for
    for (int ci=0;ci < channels;ci++) {
        chan_t &c = ch[ci];
        for (int i=0;i < fft;i++)
            c.buf[i] = c.in[i] * win[i];
        c.fwd_fn(c.fwd,c.spec,c.buf,sizeof(float));
        memcpy(c.X.data() + (size_t)slot * bins,c.spec,sizeof(AVComplexFloat) * bins);
    }

    if (!filtering) {
        #pragma omp parallel for
        for (int ci=0;ci < channels;ci++) {
            chan_t &c = ch[ci];
            std::vector<float> P(bins),Pf(bins);
            std::vector<double> cum;
            const AVComplexFloat *X = c.X.data() + (size_t)slot * bins;
            for (int j=0;j < bins;j++)
                P[j] = X[j].re * X[j].re + X[j].im * X[j].im;
            box(P,cum,nscale,Pf.data());
            for (int j=0;j < bins;j++)
                c.block[j] += Pf[j];
        }
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

        frames++;
        return;
    }

    // where this frame is in the blocks of the noise level, the middle of block b being at b*B + (B-1)/2
    int b0 = 0,b1 = 0;
    float bf = 0;
    if (nblocks > 0) {
        const double pos = std::max(0.0,std::min((f - (block_frames - 1) * 0.5) / block_frames,(double)(nblocks - 1)));
        b0 = (int)floor(pos);
        b1 = std::min(b0 + 1,nblocks - 1);
        bf = (float)(pos - b0);
    }

    const int nscales = (int)scales.size();
    const int64_t c = f - 3;                        // the frame whose gain is decided now
    const int64_t d = f - 4;                        // the frame whose gain is smoothed and put on now

    #pragma omp parallel for
    for (int di=0;di < (int)dec.size();di++) {
        decide_t &D = dec[di];

        // power and noise of the frame, of the channel or of all of them together
        std::fill(D.P.begin(),D.P.end(),0.0f);
        std::fill(D.N.begin(),D.N.end(),0.0f);
        for (int ci=0;ci < channels;ci++) {
            if (!p.link && ci != di)
                continue;

            const AVComplexFloat *X = ch[ci].X.data() + (size_t)slot * bins;
            for (int j=0;j < bins;j++)
                D.P[j] += X[j].re * X[j].re + X[j].im * X[j].im;
            if (nblocks > 0) {
                const float *n0 = ch[ci].blocks.data() + (size_t)b0 * bins,*n1 = ch[ci].blocks.data() + (size_t)b1 * bins;
                for (int j=0;j < bins;j++)
                    D.N[j] += n0[j] + bf * (n1[j] - n0[j]);
            }
        }

        // over the frequencies of each tile, then over its frames: centered on frame c
        for (int s=0;s < nscales;s++) {
            const scale_t &S = scales[s];
            float *pf = D.Pf.data() + ((size_t)s * R + slot) * bins;
            float *nf = D.Nf.data() + ((size_t)s * R + slot) * bins;
            box(D.P,D.cum,S,pf);
            box(D.N,D.cum,S,nf);

            double *ps = D.Ps.data() + (size_t)s * bins,*ns = D.Ns.data() + (size_t)s * bins;
            const int64_t add = c + S.ht,sub = c - S.ht - 1;
            if (add >= 0) {
                const float *ap = D.Pf.data() + ((size_t)s * R + (add % R)) * bins;
                const float *an = D.Nf.data() + ((size_t)s * R + (add % R)) * bins;
                for (int j=0;j < bins;j++) { ps[j] += ap[j]; ns[j] += an[j]; }
            }
            if (sub >= 0) {
                const float *sp = D.Pf.data() + ((size_t)s * R + (sub % R)) * bins;
                const float *sn = D.Nf.data() + ((size_t)s * R + (sub % R)) * bins;
                for (int j=0;j < bins;j++) { ps[j] -= sp[j]; ns[j] -= sn[j]; }
            }
        }

        // the gain of frame c: as much as the biggest of its tiles is over the threshold
        if (c >= 0) {
            float *gr = D.gr.data() + (size_t)(c % R) * bins;
            for (int j=0;j < bins;j++) {
                float g = 0;
                for (int s=0;s < nscales;s++) {
                    const double P = D.Ps[(size_t)s * bins + j],N = D.Ns[(size_t)s * bins + j];
                    if (P <= 0) {
                        if (N <= 0) g = 1;
                        continue;
                    }
                    const double r = 1 - (scales[s].lambda[j] * N) / P;
                    if (r > 0)
                        g = std::max(g,(float)sqrt(r));
                }
                gr[j] = g;
            }
        }

        // the gain of frame d, smoothed over the frames and frequencies either side, and down to the floor at most
        if (d >= 0) {
            const int64_t t0 = std::max((int64_t)0,d - 1),t1 = c;
            for (int j=0;j < bins;j++) {
                const int j0 = std::max(0,j - 1),j1 = std::min(bins - 1,j + 1);
                double sum = 0;
                for (int64_t t=t0;t <= t1;t++) {
                    const float *gr = D.gr.data() + (size_t)(t % R) * bins;
                    for (int jj=j0;jj <= j1;jj++) sum += gr[jj];
                }
                const float g = std::max(gmin,(float)(sum / ((t1 - t0 + 1) * (j1 - j0 + 1))));
                D.g[j] = 1 - weight[j] * (1 - g);
            }
        }
    }

    if (d >= 0) {
        #pragma omp parallel for
        for (int ci=0;ci < channels;ci++) {
            chan_t &C = ch[ci];
            const decide_t &D = dec[p.link ? 0 : ci];
            AVComplexFloat *spec = C.spec;
            const AVComplexFloat *X = C.X.data() + (size_t)(d % R) * bins;
            bool changed = false,floor = true;
            double e_in = 0,e_out = 0;

            for (int j=0;j < bins;j++) {
                const float g = D.g[j];
                spec[j].re = X[j].re * (g - 1);
                spec[j].im = X[j].im * (g - 1);
                if (g != 1)
                    changed = true;
                if (weight[j] >= 1) {
                    const double pw = (double)X[j].re * X[j].re + (double)X[j].im * X[j].im;
                    e_in += pw;
                    e_out += pw * g * g;
                    if (g > gmin * 1.001f)
                        floor = false;
                }
            }
            // where it was all noise, how much it was turned down by, not counting digital silence
            if (floor && e_in > 1e-9 && e_out > 0)
                C.reduced.push_back((float)(10 * log10(e_in / e_out)));

            if (changed) {
                C.inv_fn(C.inv,C.buf,spec,sizeof(AVComplexFloat));
                for (int i=0;i < fft;i++)
                    C.acc[i] += C.buf[i] * win[i] * (1.0f / 1.5f);
            }
        }

        // the first hop of acc is complete, the change to the input from (d-3)*hop on
        if (d >= 3) {
            const int64_t n = std::min((int64_t)hop,samples_in - samples_out);
            for (int ci=0;ci < channels;ci++) {
                chan_t &C = ch[ci];
                for (int64_t i=0;i < n;i++)
                    out[ci].push_back(C.dly[C.dly_at + i] + C.acc[i]);
                C.dly_at += hop;
                if (C.dly_at >= 65536) {
                    C.dly.erase(C.dly.begin(),C.dly.begin() + C.dly_at);
                    C.dly_at = 0;
                }
            }
            samples_out += n;
        }
        for (auto &C : ch) {
            memmove(C.acc.data(),C.acc.data() + hop,sizeof(float) * (fft - hop));
            memset(C.acc.data() + (fft - hop),0,sizeof(float) * hop);
        }
    }

    frames++;
}

void denoise_t::analyze(const float *const *x,int n) {
    samples_in += n;
    feed(x,n,false,NULL);
}

void denoise_t::end_analysis(void) {
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

    // ready for the second pass
    const int nd = p.link ? 1 : channels;
    dec.assign(nd,decide_t());
    for (auto &D : dec) {
        D.P.assign(bins,0);
        D.N.assign(bins,0);
        D.Pf.assign(scales.size() * R * bins,0);
        D.Nf.assign(scales.size() * R * bins,0);
        D.Ps.assign(scales.size() * bins,0);
        D.Ns.assign(scales.size() * bins,0);
        D.gr.assign((size_t)R * bins,0);
        D.g.assign(bins,1);
    }
    for (auto &c : ch) {
        std::fill(c.in.begin(),c.in.end(),0.0f);
        std::fill(c.X.begin(),c.X.end(),AVComplexFloat{0,0});
    }
    fill = 0;
    frames = samples_in = samples_out = 0;
}

void denoise_t::find_noise(void) {
    const double block_seconds = ((double)hop * block_frames) / rate;
    const int W = std::max(1,(int)lrint((p.window * 0.5) / block_seconds));
    std::vector<float> noise;
    std::vector<char> silent(nblocks);

    for (auto &c : ch) {
        noise.assign((size_t)nblocks * bins,0);

        // blocks of digital silence (before and after the recording) say nothing of the noise
        for (int b=0;b < nblocks;b++) {
            double s = 0;
            for (int j=0;j < bins;j++) s += c.blocks[(size_t)b * bins + j];
            silent[b] = s < 1e-9;
        }

        #pragma omp parallel for schedule(dynamic,16)
        for (int b=0;b < nblocks;b++) {
            const int lo = std::max(0,b - W),hi = std::min(nblocks - 1,b + W);
            std::vector<float> v;

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
                noise[(size_t)b * bins + j] = v[at] / nq[j];
            }
        }

        c.blocks.swap(noise);
    }
}

void denoise_t::filter(const float *const *x,int n,std::vector<float> *out) {
    samples_in += n;
    feed(x,n,true,out);
}

void denoise_t::end_filter(std::vector<float> *out) {
    while (samples_out < samples_in)
        feed(NULL,hop,true,out);
}

// power of the windowed transform to mean square of the samples: sum of both sides of |X|^2 is
// fft * sum((x*w)^2), and the mean of w^2 is 3/8
static double power_to_db(double pw,int fft) {
    const double ms = (2 * pw) / ((double)fft * fft * 0.375);
    return 10 * log10(ms + 1e-30);
}

double denoise_t::noise_db_at(int ci,double t) const {
    if (nblocks <= 0)
        return -300;

    const int b = std::max(0,std::min(nblocks - 1,(int)((t * rate) / ((double)hop * block_frames))));
    double s = 0;

    for (int j=0;j < bins;j++) {
        if (weight[j] >= 0.5)
            s += ch[ci].blocks[(size_t)b * bins + j];
    }

    return power_to_db(s,fft);
}

double denoise_t::noise_db(int ci) const {
    std::vector<double> v;

    for (int b=0;b < nblocks;b++)
        v.push_back(noise_db_at(ci,((double)b * hop * block_frames) / rate));
    if (v.empty())
        return -300;

    std::nth_element(v.begin(),v.begin() + v.size() / 2,v.end());
    return v[v.size() / 2];
}

double denoise_t::reduced_db(int ci) const {
    std::vector<float> v = ch[ci].reduced;

    if (v.empty())
        return 0;

    std::nth_element(v.begin(),v.begin() + v.size() / 2,v.end());
    return v[v.size() / 2];
}
