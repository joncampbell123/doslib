/* denoise.cpp: turning down the hiss and noise of a cassette tape recording (see denoise.h).
 *
 * Each channel is cut into frames of fft samples, a quarter of a frame apart, each one Hann windowed and
 * transformed. The gain of a frame is decided from the frames up to three after it (the biggest tiles),
 * and smoothed with the frames either side of it, so the frames are put out four frames after they come in.
 * Only the change to each frequency is transformed back: it is windowed again and the frames added up (four
 * Hann squared windows a quarter apart add up to 1.5 everywhere), and added to the input, which is kept back
 * to line up with it. The output lines up with the input sample for sample, and is as long.
 *
 * With -iterate, each step of making the gain finer is two frames after the one before it (the frames
 * either side of it are smoothed together), so the frames are put out up to eight frames after they come in.
 *
 * The channels are done at once, with OpenMP, each with its own transforms. */

#include <string.h>
#include <math.h>

#include <algorithm>

extern "C" {
#include <libavutil/mem.h>
}

#include "denoise.h"

static const int R = 16;                            // frames kept: more than the 9 to 13 from f-8 (f-12 with -iterate) to f that are used

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

    // -attacks: octave bands from 300Hz
    aedges.clear();
    for (double f=300;f < rate * 0.5;f *= 2)
        aedges.push_back(f);
    aband.assign(bins,-1);
    for (int j=0;j < bins;j++) {
        const double f = j * binhz;
        if (f >= 300 && !aedges.empty())
            aband[j] = std::min((int)aedges.size() - 1,(int)floor(log2(f / 300)));
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
    refine = std::min(p.iterate,2);

    // finding the noise again: noise alone over 3 frequencies and 3 frames is about Gamma(3) over 3; under
    // its 99% point, and the mean of it under that, over its mean (by numbers)
    {
        const double k = (3 / 1.5) * (3 / 2.0);
        clip = (float)gamma_quantile(k,normal_deviate(0.99));
        double s0 = 0,s1 = 0;
        for (int i=1;i <= 100000;i++) {
            const double x = (clip * (i - 0.5)) / 100000;
            const double pdf = exp(k * log(k) + (k - 1) * log(x) - k * x - lgamma(k));
            s0 += pdf;
            s1 += pdf * x;
        }
        clip_mean = (float)(s1 / s0);
    }
    fill = 0;
    frames = samples_in = samples_out = 0;
    block_count = 0;
    nblocks = 0;
    return true;
}

void denoise_t::feed(const float *const *x,int n,pass_t pass,std::vector<float> *out) {
    int at = 0;

    while (at < n) {
        const int todo = std::min(n - at,hop - fill);

        for (int c=0;c < channels;c++) {
            float *d = ch[c].in.data() + (fft - hop) + fill;
            if (x != NULL)
                memcpy(d,x[c] + at,sizeof(float) * todo);
            else
                memset(d,0,sizeof(float) * todo);
            if (pass == FILTER && !p.change_only)
                ch[c].dly.insert(ch[c].dly.end(),d,d + todo);
        }

        fill += todo;
        at += todo;
        if (fill == hop) {
            frame(pass,out);
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

// frame f in: the gain of a channel (or of all of them, with -link), up to the frame it is put on now
void denoise_t::decide(decide_t &D,int di,int64_t f,pass_t pass) {
    const int slot = (int)(f % R);
    const int nscales = (int)scales.size();
    const int64_t c = f - 3;                        // the frame whose gain is decided now

    // where this frame is in the blocks of the noise level, the middle of block b being at b*B + (B-1)/2
    int b0 = 0,b1 = 0;
    float bf = 0;
    if (nblocks > 0) {
        const double pos = std::max(0.0,std::min((f - (block_frames - 1) * 0.5) / block_frames,(double)(nblocks - 1)));
        b0 = (int)floor(pos);
        b1 = std::min(b0 + 1,nblocks - 1);
        bf = (float)(pos - b0);
    }

    // power and noise of the frame, of the channel or of all of them together
    float *Pr = D.Pr.data() + (size_t)slot * bins,*Nr = D.Nr.data() + (size_t)slot * bins;
    std::fill(Pr,Pr + bins,0.0f);
    std::fill(Nr,Nr + bins,0.0f);
    for (int ci=0;ci < channels;ci++) {
        if (!p.link && ci != di)
            continue;

        const AVComplexFloat *X = ch[ci].X.data() + (size_t)slot * bins;
        for (int j=0;j < bins;j++)
            Pr[j] += X[j].re * X[j].re + X[j].im * X[j].im;
        if (nblocks > 0) {
            const float *n0 = ch[ci].blocks.data() + (size_t)b0 * bins,*n1 = ch[ci].blocks.data() + (size_t)b1 * bins;
            for (int j=0;j < bins;j++)
                Nr[j] += n0[j] + bf * (n1[j] - n0[j]);
        }
    }
    D.P.assign(Pr,Pr + bins);
    D.N.assign(Nr,Nr + bins);

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

    if (c < 0)
        return;

    // the gain of frame c: as much as the biggest of its tiles is over the threshold
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

    // the mean of a ring over frames t-1 to t+1 and the frequencies either side of j, and of a ring squared
    // times the power, and squared
    auto mean3 = [&](const float *ring,int64_t t,int j) -> float {
        const int64_t t0 = std::max((int64_t)0,t - 1);
        const int j0 = std::max(0,j - 1),j1 = std::min(bins - 1,j + 1);
        double sum = 0;
        for (int64_t tt=t0;tt <= t + 1;tt++) {
            const float *r = ring + (size_t)(tt % R) * bins;
            for (int jj=j0;jj <= j1;jj++) sum += r[jj];
        }
        return (float)(sum / ((t + 1 - t0 + 1) * (j1 - j0 + 1)));
    };

    if (pass == REESTIMATE) {
        // each frequency of frame c that nothing was let through at, and that is not louder than the noise by more
        // than chance over the frequencies and frames around it, is noise alone
        const int blk = (int)std::min((int64_t)nblocks - 1,c / block_frames);
        const float *Nc = D.Nr.data() + (size_t)(c % R) * bins;
        if (blk < 0)
            return;
        for (int j=0;j < bins;j++) {
            if (gr[j] > 0 || mean3(D.Pr.data(),c,j) >= clip * Nc[j])
                continue;
            for (int ci=0;ci < channels;ci++) {
                if (!p.link && ci != di)
                    continue;
                const AVComplexFloat &x = ch[ci].X[(size_t)(c % R) * bins + j];
                ch[ci].esum[(size_t)blk * bins + j] += x.re * x.re + x.im * x.im;
                ch[ci].ecnt[(size_t)blk * bins + j] += 1;
            }
        }
        return;
    }

    // the gain, smoothed over the frames and frequencies either side, and down to the floor at most: frame c-1
    {
        const int64_t t = c - 1;
        if (t >= 0) {
            float *G0 = D.G.data() + (size_t)(t % R) * bins;
            for (int j=0;j < bins;j++)
                G0[j] = std::max(gmin,mean3(D.gr.data(),t,j));
        }
    }

    // made finer: let through by as much of it as is the sound let through by the last step, less the noise let
    // through with it, up to as much as the tiles let through, and smoothed again; each step two frames later
    for (int s=1;s <= refine;s++) {
        const float *Gp = D.G.data() + (size_t)(s - 1) * R * bins;
        float *hs = D.h.data() + (size_t)s * R * bins;
        float *Gs = D.G.data() + (size_t)s * R * bins;
        const int64_t th = c - 2 * s;                   // the last step has its gain up to c+1-2s
        const int64_t tg = th - 1;

        if (th >= 0) {
            const int64_t t0 = std::max((int64_t)0,th - 1);
            const float *Nt = D.Nr.data() + (size_t)(th % R) * bins;
            const float *gt = D.gr.data() + (size_t)(th % R) * bins;
            float *ht = hs + (size_t)(th % R) * bins;
            for (int j=0;j < bins;j++) {
                const int j0 = std::max(0,j - 1),j1 = std::min(bins - 1,j + 1);
                double sp = 0,sg = 0;
                for (int64_t tt=t0;tt <= th + 1;tt++) {
                    const float *g = Gp + (size_t)(tt % R) * bins,*pw = D.Pr.data() + (size_t)(tt % R) * bins;
                    for (int jj=j0;jj <= j1;jj++) {
                        const double g2 = (double)g[jj] * g[jj];
                        sp += g2 * pw[jj];
                        sg += g2;
                    }
                }
                const double n = (th + 1 - t0 + 1) * (j1 - j0 + 1);
                const double S = std::max(0.0,(sp - sg * Nt[j]) / n);
                const float w = (S + Nt[j]) > 0 ? (float)(S / (S + Nt[j])) : 1.0f;
                ht[j] = std::min(gt[j],w);
            }
        }
        if (tg >= 0) {
            float *gs = Gs + (size_t)(tg % R) * bins;
            for (int j=0;j < bins;j++)
                gs[j] = std::max(gmin,mean3(hs,tg,j));
        }
    }

    // the gain put on now, only from -from up; with -attacks, only this transform's part of the change
    const int64_t d = f - 4 - 2 * refine;
    if (d >= 0) {
        const float *Gd = D.G.data() + ((size_t)refine * R + (d % R)) * bins;
        if (amap != NULL) {
            std::vector<float> aw;
            attack_weights(di,d,aw);
            for (int j=0;j < bins;j++) {
                const float part = aband[j] >= 0 ? aw[aband[j]] : (ashort ? 0.0f : 1.0f);
                D.g[j] = 1 - weight[j] * part * (1 - Gd[j]);
            }
        }
        else {
            for (int j=0;j < bins;j++)
                D.g[j] = 1 - weight[j] * (1 - Gd[j]);
        }
    }
}

void denoise_t::frame(pass_t pass,std::vector<float> *out) {
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

    if (pass == ANALYZE) {
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

            // -attacks: the power of each octave band, for finding the attacks once the noise is known
            if (p.attacks) {
                std::vector<double> bp(aedges.size(),0);
                for (int j=0;j < bins;j++)
                    if (aband[j] >= 0) bp[aband[j]] += P[j];
                for (double v : bp)
                    c.bandpow.push_back((uint16_t)std::max(0.0,std::min(65535.0,(10 * log10(v + 1e-30) + 200) * 256)));
            }
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

    #pragma omp parallel for
    for (int di=0;di < (int)dec.size();di++)
        decide(dec[di],di,f,pass);

    const int64_t d = f - 4 - 2 * refine;           // the frame whose gain is put on now
    if (pass == FILTER && d >= 0) {
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
                if (p.change_only) {
                    out[ci].insert(out[ci].end(),C.acc.begin(),C.acc.begin() + n);
                    continue;
                }
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
    feed(x,n,ANALYZE,NULL);
}

// the frames that the filter will have at the end, over the last of it
void denoise_t::end_pass(pass_t pass) {
    feed(NULL,fft - hop + (hop - fill) % hop,pass,NULL);
}

// ready for the next pass, from the start
void denoise_t::reset(void) {
    const int nd = p.link ? 1 : channels;
    dec.assign(nd,decide_t());
    for (auto &D : dec) {
        D.P.assign(bins,0);
        D.N.assign(bins,0);
        D.Pr.assign((size_t)R * bins,0);
        D.Nr.assign((size_t)R * bins,0);
        D.Pf.assign(scales.size() * R * bins,0);
        D.Nf.assign(scales.size() * R * bins,0);
        D.Ps.assign(scales.size() * bins,0);
        D.Ns.assign(scales.size() * bins,0);
        D.gr.assign((size_t)R * bins,0);
        D.G.assign((size_t)(refine + 1) * R * bins,0);
        D.h.assign((size_t)(refine + 1) * R * bins,0);
        D.g.assign(bins,1);
    }
    for (auto &c : ch) {
        std::fill(c.in.begin(),c.in.end(),0.0f);
        std::fill(c.X.begin(),c.X.end(),AVComplexFloat{0,0});
    }
    fill = 0;
    frames = samples_in = samples_out = 0;
}

void denoise_t::end_analysis(void) {
    end_pass(ANALYZE);
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
    reset();
}

void denoise_t::reestimate(const float *const *x,int n) {
    if (ch[0].esum.empty()) {
        for (auto &c : ch) {
            c.esum.assign((size_t)nblocks * bins,0);
            c.ecnt.assign((size_t)nblocks * bins,0);
        }
    }
    samples_in += n;
    feed(x,n,REESTIMATE,NULL);
}

void denoise_t::end_reestimate(void) {
    end_pass(REESTIMATE);
    find_noise_again();
    reset();
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


// the noise level again, from the frequencies of the moments that were noise alone: over the window around each
// block, and 1/12 octave around each frequency, their mean, made up for the ones over the clip left out; where
// there are too few of them, as it was
void denoise_t::find_noise_again(void) {
    const double block_seconds = ((double)hop * block_frames) / rate;
    const int W = std::max(1,(int)lrint((p.window * 0.5) / block_seconds));

    for (auto &c : ch) {
        if (c.esum.size() != (size_t)nblocks * bins)
            continue;

        // over frequency
        std::vector<double> s((size_t)nblocks * bins),n((size_t)nblocks * bins),cum(bins + 1);
        for (int b=0;b < nblocks;b++) {
            for (int pass=0;pass < 2;pass++) {
                const float *src = (pass ? c.ecnt.data() : c.esum.data()) + (size_t)b * bins;
                double *dst = (pass ? n.data() : s.data()) + (size_t)b * bins;
                cum[0] = 0;
                for (int j=0;j < bins;j++) cum[j + 1] = cum[j] + src[j];
                for (int j=0;j < bins;j++) dst[j] = cum[nscale.hi[j] + 1] - cum[nscale.lo[j]];
            }
        }

        // over the window, by running sums
        std::vector<double> rs(bins,0),rn(bins,0);
        int lo = 0,hi = -1;
        for (int b=0;b < nblocks;b++) {
            const int nlo = std::max(0,b - W),nhi = std::min(nblocks - 1,b + W);
            while (hi < nhi) {
                hi++;
                for (int j=0;j < bins;j++) { rs[j] += s[(size_t)hi * bins + j]; rn[j] += n[(size_t)hi * bins + j]; }
            }
            while (lo < nlo) {
                for (int j=0;j < bins;j++) { rs[j] -= s[(size_t)lo * bins + j]; rn[j] -= n[(size_t)lo * bins + j]; }
                lo++;
            }
            for (int j=0;j < bins;j++) {
                if (rn[j] >= 20)
                    c.blocks[(size_t)b * bins + j] = (float)(rs[j] / rn[j] / clip_mean);
            }
        }

        std::vector<float>().swap(c.esum);
        std::vector<float>().swap(c.ecnt);
    }
}

void denoise_t::filter(const float *const *x,int n,std::vector<float> *out) {
    samples_in += n;
    feed(x,n,FILTER,out);
}

void denoise_t::end_filter(std::vector<float> *out) {
    while (samples_out < samples_in)
        feed(NULL,hop,FILTER,out);
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

/////////////////////////////////////////////////////////////////////////////
// -attacks

int attack_map_t::band_of(double hz) const {
    if (edges.empty() || hz < edges[0])
        return -1;
    return std::min((int)edges.size() - 1,(int)floor(log2(hz / edges[0])));
}

// the part of the change the shorter transform takes at time t: 1 from before an attack to after it, faded
float attack_map_t::weight(int ch,int band,double t) const {
    if (ch < 0 || ch >= (int)onsets.size() || band < 0 || band >= (int)onsets[ch].size())
        return 0;

    const std::vector<double> &on = onsets[ch][band];
    float w = 0;
    for (auto i = std::lower_bound(on.begin(),on.end(),t - after - ramp);i != on.end() && *i <= t + before + ramp;i++) {
        double v = 1;
        if (t < *i - before) v = 1 - ((*i - before) - t) / ramp;
        else if (t > *i + after) v = 1 - (t - (*i + after)) / ramp;
        w = std::max(w,(float)std::max(0.0,v));
    }
    return w;
}

size_t attack_map_t::count(void) const {
    size_t n = 0;
    for (const auto &c : onsets)
        for (const auto &b : c) n += b.size();
    return n;
}

void denoise_t::find_attacks(attack_map_t &m) const {
    const int nb = (int)aedges.size();
    const int kb = std::max(1,(int)lrint((m.lookback * rate) / hop));
    const float rise = (float)pow(10.0,m.rise_db / 10);

    m.rate = rate;
    m.edges = aedges;
    m.onsets.assign(channels,std::vector<std::vector<double>>(nb));

    // over the noise by more than chance: as many values as the band has frequencies
    std::vector<float> lam(nb);
    std::vector<int> nbin(nb,0);
    for (int j=0;j < bins;j++)
        if (aband[j] >= 0) nbin[aband[j]]++;
    for (int b=0;b < nb;b++)
        lam[b] = (float)(gamma_quantile(std::max(1.0,nbin[b] / 1.5),p.threshold) * pow(10.0,p.margin / 10));

    for (int ci=0;ci < channels;ci++) {
        const chan_t &c = ch[ci];
        const int64_t nf = (int64_t)(c.bandpow.size() / std::max(1,nb));

        // the noise of each band, each block
        std::vector<float> nband((size_t)nblocks * nb,0);
        for (int blk=0;blk < nblocks;blk++)
            for (int j=0;j < bins;j++)
                if (aband[j] >= 0) nband[(size_t)blk * nb + aband[j]] += c.blocks[(size_t)blk * bins + j];

        for (int b=0;b < nb;b++) {
            std::vector<double> &on = m.onsets[ci][b];
            std::vector<float> E(nf);
            for (int64_t f=0;f < nf;f++)
                E[f] = (float)pow(10.0,(c.bandpow[(size_t)f * nb + b] / 256.0 - 200) / 10);

            for (int64_t f=1;f < nf;f++) {
                float prev = 0;
                for (int64_t k=std::max((int64_t)0,f - kb);k < f;k++) prev = std::max(prev,E[k]);
                if (E[f] <= prev * rise)
                    continue;

                const double pos = std::max(0.0,std::min((f - (block_frames - 1) * 0.5) / block_frames,(double)(nblocks - 1)));
                const int b0 = (int)floor(pos),b1 = std::min(b0 + 1,nblocks - 1);
                const float En = nband[(size_t)b0 * nb + b] + (float)(pos - b0) * (nband[(size_t)b1 * nb + b] - nband[(size_t)b0 * nb + b]);
                if (E[f] <= lam[b] * En)
                    continue;

                // the middle of frame f, which is the samples from (f-3)*hop
                const double t = ((f - 3) * (double)hop + fft * 0.5) / rate;
                if (on.empty() || t - on.back() > 0.005)
                    on.push_back(t);
            }
        }
    }
}

void denoise_t::use_attacks(const attack_map_t *m,bool shorter) {
    amap = m;
    ashort = shorter;
    for (int j=0;j < bins;j++)
        aband[j] = m != NULL ? m->band_of((double)j * rate / fft) : -1;
}

// this transform's part of the change of frame d, each octave band: the shorter one at the middle of its frame,
// the longer one 1 less the shorter's over its frame, as its window weighs it; of the channels the gain is for
void denoise_t::attack_weights(int di,int64_t d,std::vector<float> &w) const {
    const int nb = (int)amap->edges.size();
    const double s0 = (d - 3) * (double)hop;

    w.assign(nb,0);
    for (int ci=0;ci < channels;ci++) {
        if (!p.link && ci != di)
            continue;
        for (int b=0;b < nb;b++) {
            float a;
            if (ashort) {
                a = amap->weight(ci,b,(s0 + fft * 0.5) / rate);
            }
            else {
                double sw = 0,sa = 0;
                for (int i=0;i < 16;i++) {
                    const double x = (i + 0.5) / 16,h = 0.5 - 0.5 * cos(2 * M_PI * x),h2 = h * h;
                    sa += h2 * amap->weight(ci,b,(s0 + x * fft) / rate);
                    sw += h2;
                }
                a = (float)(sa / sw);
            }
            w[b] = std::max(w[b],a);
        }
    }
    if (!ashort) {
        for (auto &v : w) v = 1 - v;
    }
}
