/* level.cpp: taking out the small rapid changes of level that go with the rumble (see level.h).
 *
 * Each channel is cut into frames of about 5ms, a quarter of a frame apart (about 1.3ms at 48000Hz), as
 * short as they are so that a change of level 100 times a second can be followed. The level of each band
 * of half an octave from 300Hz up, in dB, one to a frame, is compared with the waveform of the rumble
 * (20 to 250Hz) of each channel, sampled one to a frame, in segments of about 2.7 seconds, half a segment
 * apart, through their transforms: for each rate of change from 5 to 100 times a second, how the level
 * goes with the rumble (the cross spectrum) and how much it goes by itself.
 *
 * The segments are summed into blocks of 30 seconds, and for each block, over the blocks around it (the
 * window), the filter from the rumble of each channel to the level of each band is found, at each rate of
 * change, as the one that predicts the most of it (least squares). Where the rumble predicts only a little
 * of the level, which is most of what is found by chance, the filter is turned down or off.
 *
 * The second time through, the change of level that each block's filter predicts from the rumble is
 * worked out over the block and half of each block next to it (by transforms of about 87 seconds),
 * and between the middles of two blocks it is faded from the one to the other. Each frequency of each
 * frame is turned the other way by the change of level of the bands around it. As in derumble.cpp,
 * only the change is transformed back, and added to the input. */

#include <string.h>
#include <math.h>

#include <algorithm>
#include <complex>

extern "C" {
#include <libavutil/mem.h>
}

#include "level.h"

typedef std::complex<double> cplx;

static const double block_seconds = 30;
static const double segment_seconds = 2.7;
static const double rate_lo = 5,rate_hi = 100;      // the rates of change of level looked at, per second
static const int smooth_bins = 2;                   // rates of change either side summed with each one
static const float explained_min = 0.01f;           // a block where the rumble accounts for less of the change of level is left alone

void biquad_t::lowpass(double f,double q,int rate) {
    const double w = (2 * M_PI * f) / rate,cw = cos(w),alpha = sin(w) / (2 * q),a0 = 1 + alpha;
    b0 = ((1 - cw) / 2) / a0; b1 = (1 - cw) / a0; b2 = b0;
    a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
}

void biquad_t::highpass(double f,double q,int rate) {
    const double w = (2 * M_PI * f) / rate,cw = cos(w),alpha = sin(w) / (2 * q),a0 = 1 + alpha;
    b0 = ((1 + cw) / 2) / a0; b1 = -(1 + cw) / a0; b2 = b0;
    a1 = (-2 * cw) / a0; a2 = (1 - alpha) / a0;
}

static int pow2_at_least(double v,int min) {
    int n = min;
    while (n < v) n *= 2;
    return n;
}

static bool tx_init(AVTXContext **ctx,av_tx_fn *fn,int inverse,int len) {
    float scale = inverse ? 1.0f / len : 1.0f;
    return av_tx_init(ctx,fn,AV_TX_FLOAT_RDFT,inverse,len,&scale,0) >= 0;
}

leveler_t::~leveler_t() {
    av_tx_uninit(&fwd); av_tx_uninit(&inv);
    av_tx_uninit(&sfwd); av_tx_uninit(&sinv);
    av_tx_uninit(&pfwd); av_tx_uninit(&pinv);
    av_freep(&buf); av_freep(&spec);
    av_freep(&sbuf); av_freep(&sspec);
    av_freep(&pbuf); av_freep(&pspec);
    av_freep(&rspec); av_freep(&macc);
}

bool leveler_t::init(const level_params_t &np,int sample_rate,int nchannels) {
    p = np;
    rate = sample_rate;
    channels = nchannels;
    if (rate <= 0 || channels <= 0)
        return false;

    fft = pow2_at_least(rate * 0.005,64);
    hop = fft / 4;
    const double frate = (double)rate / hop;

    // bands of half an octave from 300Hz, up to 16kHz or near the top
    const double top = std::min(16000.0,rate * 0.45);
    edges.clear();
    for (double f=300;f <= top;f *= M_SQRT2)
        edges.push_back(f);
    nbands = (int)edges.size() - 1;
    if (nbands < 1)
        return false;

    // each frequency: where it is between the middles of the bands, in log frequency, none below 250Hz
    std::vector<double> mid(nbands);
    for (int b=0;b < nbands;b++)
        mid[b] = sqrt(edges[b] * edges[b + 1]);
    bin_band.assign(fft / 2 + 1,-1);
    bin_frac.assign(fft / 2 + 1,0);
    for (int j=0;j <= fft / 2;j++) {
        const double f = ((double)j * rate) / fft;
        if (f < 250)
            continue;
        if (f <= mid[0]) {
            bin_band[j] = 0;
        }
        else if (f >= mid[nbands - 1]) {
            bin_band[j] = nbands - 1;
        }
        else {
            int b = 0;
            while (b < nbands - 2 && f >= mid[b + 1]) b++;
            bin_band[j] = b;
            bin_frac[j] = (float)(log(f / mid[b]) / log(mid[b + 1] / mid[b]));
        }
    }

    win.resize(fft);
    for (int i=0;i < fft;i++)
        win[i] = (float)(0.5 - 0.5 * cos((2 * M_PI * i) / fft));

    seglen = pow2_at_least(frate * segment_seconds,64);
    segwin.resize(seglen);
    for (int i=0;i < seglen;i++)
        segwin[i] = (float)(0.5 - 0.5 * cos((2 * M_PI * i) / seglen));
    kmin = std::max(1,(int)ceil((rate_lo * seglen) / frate));
    kmax = std::min(seglen / 2 - 1,(int)floor((rate_hi * seglen) / frate));
    if (kmax < kmin)
        return false;

    block_frames = std::max(seglen,(int)lrint(block_seconds * frate));
    pfft = pow2_at_least(2.0 * block_frames + seglen,256);

    buf = (float*)av_malloc(sizeof(float) * fft);
    spec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * (fft / 2 + 1));
    sbuf = (float*)av_malloc(sizeof(float) * seglen);
    sspec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * (seglen / 2 + 1));
    pbuf = (float*)av_malloc(sizeof(float) * pfft);
    pspec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * (pfft / 2 + 1));
    // each channel's rumble transform kept aligned for the transform, as the first one is
    rstride = ((pfft / 2 + 1) + 7) & ~7;
    rspec = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * rstride * channels);
    macc = (AVComplexFloat*)av_malloc(sizeof(AVComplexFloat) * (pfft / 2 + 1));
    if (buf == NULL || spec == NULL || sbuf == NULL || sspec == NULL || pbuf == NULL || pspec == NULL || rspec == NULL || macc == NULL)
        return false;
    if (!tx_init(&fwd,&fwd_fn,0,fft) || !tx_init(&inv,&inv_fn,1,fft) ||
        !tx_init(&sfwd,&sfwd_fn,0,seglen) || !tx_init(&sinv,&sinv_fn,1,seglen) ||
        !tx_init(&pfwd,&pfwd_fn,0,pfft) || !tx_init(&pinv,&pinv_fn,1,pfft))
        return false;

    ch.resize(channels);
    for (auto &c : ch) {
        c.in.assign(fft,0);
        c.acc.assign(fft,0);
        c.ering.assign((size_t)nbands * seglen,0);
        c.r.assign(1,0.0f);
        // the rumble band: 2nd order high pass at 20Hz, 4th order low pass at 250Hz
        c.bp[0].highpass(20,M_SQRT1_2,rate);
        c.bp[1].lowpass(250,0.5412,rate);
        c.bp[2].lowpass(250,1.3066,rate);
    }

    m.assign(nbands,0);
    mbin.assign(fft / 2 + 1,0);

    fill = 0;
    frames = samples_in = samples_out = sample_at = 0;
    nblocks = 0;
    return true;
}

void leveler_t::feed(const float *const *x,int n,bool filtering,std::vector<float> *out) {
    int at = 0;

    while (at < n) {
        const int todo = std::min(n - at,hop - fill);

        for (int ci=0;ci < channels;ci++) {
            level_chan_t &c = ch[ci];
            float *d = c.in.data() + (fft - hop) + fill;

            if (x != NULL)
                memcpy(d,x[ci] + at,sizeof(float) * todo);
            else
                memset(d,0,sizeof(float) * todo);

            // the rumble waveform, one sample of it to a frame, at the middle of the frame:
            // frame k is the samples from (k-3)*hop, its middle at (k-1)*hop
            if (!filtering) {
                for (int i=0;i < todo;i++) {
                    double y = d[i];
                    for (auto &bq : c.bp) y = bq.run(y);
                    if ((sample_at + i) % hop == 0)
                        c.r.push_back((float)y);
                }
            }
        }

        sample_at += todo;
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

const std::vector<float>& leveler_t::prediction(int k) {
    auto i = predicted.find(k);
    if (i != predicted.end())
        return i->second;

    // the ones before the one before this are done with
    while (!predicted.empty() && predicted.begin()->first < k - 1)
        predicted.erase(predicted.begin());

    const int span = 2 * block_frames;
    const int64_t base = (int64_t)k * block_frames - block_frames / 2 - seglen / 2;
    const int pbins = pfft / 2 + 1;
    std::vector<float> &P = predicted[k];

    P.assign((size_t)channels * nbands * span,0);

    for (int ri=0;ri < channels;ri++) {
        const std::vector<float> &r = ch[ri].r;
        for (int i=0;i < pfft;i++) {
            const int64_t t = base + i;
            pbuf[i] = (i < span + seglen && t >= 0 && t < (int64_t)r.size()) ? r[t] : 0.0f;
        }
        pfwd_fn(pfwd,rspec + (size_t)ri * rstride,pbuf,sizeof(float));
    }

    for (int ci=0;ci < channels;ci++) {
        if (!ch[ci].linked[k])
            continue;

        for (int b=0;b < nbands;b++) {
            std::fill(macc,macc + pbins,AVComplexFloat{0,0});

            for (int ri=0;ri < channels;ri++) {
                // the filter, taps from -seglen/2 to seglen/2, around in a transform of pfft
                const float *h = ch[ci].h.data() + ((((size_t)k * nbands + b) * channels + ri) * seglen);
                memset(pbuf,0,sizeof(float) * pfft);
                for (int n=0;n < seglen;n++)
                    pbuf[n < seglen / 2 ? n : pfft - (seglen - n)] = h[n];
                pfwd_fn(pfwd,pspec,pbuf,sizeof(float));

                const AVComplexFloat *R = rspec + (size_t)ri * rstride;
                for (int j=0;j < pbins;j++) {
                    macc[j].re += pspec[j].re * R[j].re - pspec[j].im * R[j].im;
                    macc[j].im += pspec[j].re * R[j].im + pspec[j].im * R[j].re;
                }
            }

            pinv_fn(pinv,pbuf,macc,sizeof(AVComplexFloat));
            float *o = P.data() + ((size_t)ci * nbands + b) * span;
            for (int i=0;i < span;i++)
                o[i] = pbuf[seglen / 2 + i];
        }
    }

    return P;
}

void leveler_t::frame(bool filtering,std::vector<float> *out) {
    if (!filtering) {
        for (auto &c : ch) {
            for (int i=0;i < fft;i++)
                buf[i] = c.in[i] * win[i];
            fwd_fn(fwd,spec,buf,sizeof(float));

            // the level of each band, in dB
            int j = 0;
            for (int b=0;b < nbands;b++) {
                double pw = 0;
                while (j <= fft / 2 && ((double)j * rate) / fft < edges[b]) j++;
                for (int jj=j;jj <= fft / 2 && ((double)jj * rate) / fft < edges[b + 1];jj++)
                    pw += (double)spec[jj].re * spec[jj].re + (double)spec[jj].im * spec[jj].im;
                c.ering[(size_t)b * seglen + (frames % seglen)] = (float)(10 * log10(pw + 1e-9));
            }
        }

        frames++;
        if (frames >= seglen && frames % (seglen / 2) == 0)
            segment();
        return;
    }

    // the change of level of this frame: faded from the block before the middle of it to the one after
    const int span = 2 * block_frames;
    if (nblocks > 0) {
        const int64_t t = frames;
        int64_t k = (t - block_frames / 2) >= 0 ? (t - block_frames / 2) / block_frames : -1;
        int k0 = (int)std::max((int64_t)0,std::min(k,(int64_t)nblocks - 1));
        int k1 = (int)std::min(k + 1,(int64_t)nblocks - 1);
        float a = 0;

        if (k >= 0 && k + 1 < nblocks)
            a = (float)(t - (k * block_frames + block_frames / 2)) / block_frames;
        if (k < 0)
            k1 = k0;

        const std::vector<float> &P0 = prediction(k0);
        const std::vector<float> &P1 = prediction(k1);
        const int64_t i0 = std::max((int64_t)0,std::min((int64_t)span - 1,t - ((int64_t)k0 * block_frames - block_frames / 2)));
        const int64_t i1 = std::max((int64_t)0,std::min((int64_t)span - 1,t - ((int64_t)k1 * block_frames - block_frames / 2)));

        for (int ci=0;ci < channels;ci++) {
            level_chan_t &c = ch[ci];
            float mm = 0;
            bool changed = false;

            for (int b=0;b < nbands;b++) {
                const size_t o = ((size_t)ci * nbands + b) * span;
                m[b] = std::max((float)-p.max,std::min((float)p.max,(1 - a) * P0[o + i0] + a * P1[o + i1]));
                mm = std::max(mm,fabsf(m[b]));
            }
            if (mm > 0.01f) {
                c.msum += (double)mm * mm;
                c.mcount++;
                c.mmax = std::max(c.mmax,(double)mm);
                changed = true;
            }
            if (!changed)
                continue;

            for (int i=0;i < fft;i++)
                buf[i] = c.in[i] * win[i];
            fwd_fn(fwd,spec,buf,sizeof(float));

            // each frequency turned the other way by the change of level around it
            for (int j=0;j <= fft / 2;j++) {
                const int b = bin_band[j];
                float g = 1;

                if (b >= 0) {
                    const float d = b + 1 < nbands ? m[b] + bin_frac[j] * (m[b + 1] - m[b]) : m[b];
                    g = powf(10.0f,-d / 20);
                }
                spec[j].re *= g - 1;
                spec[j].im *= g - 1;
            }

            inv_fn(inv,buf,spec,sizeof(AVComplexFloat));
            for (int i=0;i < fft;i++)
                c.acc[i] += buf[i] * win[i] * (1.0f / 1.5f);
        }
    }

    // the first hop of acc is complete, the change to the first hop of this frame's input;
    // the first three frames are before the start
    if (frames >= 3) {
        const int64_t n = std::min((int64_t)hop,samples_in - samples_out);
        for (int ci=0;ci < channels;ci++) {
            const level_chan_t &c = ch[ci];
            for (int64_t i=0;i < n;i++)
                out[ci].push_back(c.in[i] + c.acc[i]);
        }
        samples_out += n;
    }
    for (auto &c : ch) {
        memmove(c.acc.data(),c.acc.data() + hop,sizeof(float) * (fft - hop));
        memset(c.acc.data() + (fft - hop),0,sizeof(float) * hop);
    }

    frames++;
}

void leveler_t::segment(void) {
    const int64_t f0 = frames - seglen;
    const int block = (int)((f0 + seglen / 2) / block_frames);
    const int K = kmax - kmin + 1;
    const int C = channels,B = nbands;

    while ((int)sums.size() <= block) {
        sums.emplace_back();
        block_sums_t &s = sums.back();
        s.lr.assign((size_t)C * B * C * K * 2,0);
        s.rr.assign((size_t)C * C * K * 2,0);
        s.ll.assign((size_t)C * B * K,0);
    }
    nblocks = (int)sums.size();
    block_sums_t &s = sums[block];
    s.segs++;

    // the rumble of each channel, transformed
    std::vector<cplx> R((size_t)C * K);
    for (int ri=0;ri < C;ri++) {
        const std::vector<float> &r = ch[ri].r;
        double mean = 0;
        for (int i=0;i < seglen;i++) {
            const int64_t t = f0 + i;
            sbuf[i] = t < (int64_t)r.size() ? r[t] : 0.0f;
            mean += sbuf[i];
        }
        mean /= seglen;
        for (int i=0;i < seglen;i++)
            sbuf[i] = (float)((sbuf[i] - mean) * segwin[i]);
        sfwd_fn(sfwd,sspec,sbuf,sizeof(float));
        for (int k=0;k < K;k++)
            R[(size_t)ri * K + k] = cplx(sspec[kmin + k].re,sspec[kmin + k].im);
    }

    for (int ri=0;ri < C;ri++) {
        for (int rj=0;rj < C;rj++) {
            double *d = s.rr.data() + ((size_t)ri * C + rj) * K * 2;
            for (int k=0;k < K;k++) {
                const cplx v = R[(size_t)ri * K + k] * std::conj(R[(size_t)rj * K + k]);
                d[k * 2] += v.real();
                d[k * 2 + 1] += v.imag();
            }
        }
    }

    // the level of each band of each channel, transformed, against the rumble
    for (int ci=0;ci < C;ci++) {
        for (int b=0;b < B;b++) {
            const float *e = ch[ci].ering.data() + (size_t)b * seglen;
            double mean = 0;
            for (int i=0;i < seglen;i++) {
                sbuf[i] = e[(f0 + i) % seglen];
                mean += sbuf[i];
            }
            mean /= seglen;
            for (int i=0;i < seglen;i++)
                sbuf[i] = (float)((sbuf[i] - mean) * segwin[i]);
            sfwd_fn(sfwd,sspec,sbuf,sizeof(float));

            double *ll = s.ll.data() + ((size_t)ci * B + b) * K;
            for (int k=0;k < K;k++) {
                const cplx A(sspec[kmin + k].re,sspec[kmin + k].im);
                ll[k] += std::norm(A);
                for (int ri=0;ri < C;ri++) {
                    const cplx v = A * std::conj(R[(size_t)ri * K + k]);
                    double *d = s.lr.data() + ((((size_t)ci * B + b) * C + ri) * K + k) * 2;
                    d[0] += v.real();
                    d[1] += v.imag();
                }
            }
        }
    }
}

// solves M x = y, n by n, for small n; false if it cannot be
static bool solve(std::vector<cplx> M,std::vector<cplx> &x,const std::vector<cplx> &y,int n) {
    x = y;
    for (int c=0;c < n;c++) {
        int piv = c;
        for (int r=c + 1;r < n;r++)
            if (std::abs(M[r * n + c]) > std::abs(M[piv * n + c])) piv = r;
        if (std::abs(M[piv * n + c]) < 1e-30)
            return false;
        if (piv != c) {
            for (int k=0;k < n;k++) std::swap(M[c * n + k],M[piv * n + k]);
            std::swap(x[c],x[piv]);
        }
        for (int r=0;r < n;r++) {
            if (r == c) continue;
            const cplx f = M[r * n + c] / M[c * n + c];
            for (int k=c;k < n;k++) M[r * n + k] -= f * M[c * n + k];
            x[r] -= f * x[c];
        }
    }
    for (int c=0;c < n;c++)
        x[c] /= M[c * n + c];
    return true;
}

void leveler_t::find_filters(void) {
    const int K = kmax - kmin + 1;
    const int C = channels,B = nbands;
    const int W = std::max(1,(int)lrint((p.window * 0.5) / block_seconds));

    for (auto &c : ch) {
        c.h.assign((size_t)nblocks * B * C * seglen,0);
        c.linked.assign(nblocks,0);
        c.explained.assign(nblocks,0);
    }

    std::vector<double> lr((size_t)C * B * C * K * 2),rr((size_t)C * C * K * 2),ll((size_t)C * B * K);
    std::vector<cplx> Hw((size_t)C * B * C * K);
    std::vector<cplx> Mx(C * C),x(C),y(C);

    for (int k=0;k < nblocks;k++) {
        const int lo = std::max(0,k - W),hi = std::min(nblocks - 1,k + W);

        std::fill(lr.begin(),lr.end(),0.0);
        std::fill(rr.begin(),rr.end(),0.0);
        std::fill(ll.begin(),ll.end(),0.0);
        int segs = 0;
        for (int kk=lo;kk <= hi;kk++) {
            segs += sums[kk].segs;
            for (size_t i=0;i < lr.size();i++) lr[i] += sums[kk].lr[i];
            for (size_t i=0;i < rr.size();i++) rr[i] += sums[kk].rr[i];
            for (size_t i=0;i < ll.size();i++) ll[i] += sums[kk].ll[i];
        }

        // what is found by chance, from as many segments and rates of change as are summed, is about
        // as many rumble channels as there are, to that many
        // too little of the recording to tell at all
        if (segs < 8)
            continue;

        const double dof = (double)segs * (2 * smooth_bins + 1);
        const double c0 = 0.05 + (3.0 * C) / dof,c1 = c0 + 0.25;

        std::fill(Hw.begin(),Hw.end(),cplx(0,0));
        std::vector<double> ex(C,0),all(C,0);
        for (int kb=0;kb < K;kb++) {
            const int s0 = std::max(0,kb - smooth_bins),s1 = std::min(K - 1,kb + smooth_bins);

            // the rumble by rumble, summed over the rates of change around this one, and held a little
            // off of nothing so that it can always be solved
            double tr = 0;
            for (int i=0;i < C;i++) {
                for (int j=0;j < C;j++) {
                    cplx v(0,0);
                    for (int s=s0;s <= s1;s++) {
                        const double *d = rr.data() + (((size_t)j * C + i) * K + s) * 2;
                        v += cplx(d[0],d[1]);
                    }
                    Mx[i * C + j] = v;      // row i: the equation for rumble channel i
                }
                tr += Mx[i * C + i].real();
            }
            if (tr <= 0)
                continue;
            for (int i=0;i < C;i++)
                Mx[i * C + i] += (1e-6 * tr) / C;

            for (int ci=0;ci < C;ci++) {
                for (int b=0;b < B;b++) {
                    double L = 0;
                    for (int s=s0;s <= s1;s++)
                        L += ll[((size_t)ci * B + b) * K + s];
                    if (L <= 0)
                        continue;

                    for (int i=0;i < C;i++) {
                        cplx v(0,0);
                        for (int s=s0;s <= s1;s++) {
                            const double *d = lr.data() + ((((size_t)ci * B + b) * C + i) * K + s) * 2;
                            v += cplx(d[0],d[1]);
                        }
                        y[i] = v;
                    }
                    // sum over rumble channels j of H[j] * (rumble j by rumble i) = level by rumble i
                    if (!solve(Mx,x,y,C))
                        continue;

                    // how much of the level the rumble predicts
                    double explained = 0;
                    for (int i=0;i < C;i++)
                        explained += (x[i] * std::conj(y[i])).real();
                    const double coh = explained / L;
                    const double w = std::max(0.0,std::min(1.0,(coh - c0) / (c1 - c0)));

                    ex[ci] += w * explained;
                    all[ci] += L;

                    if (w > 0) {
                        for (int i=0;i < C;i++)
                            Hw[(((size_t)ci * B + b) * C + i) * K + kb] = x[i] * w;
                    }
                }
            }
        }

        // where the rumble accounts for hardly any of the change of level, it is left as it is
        for (int ci=0;ci < C;ci++) {
            ch[ci].explained[k] = all[ci] > 0 ? (float)(ex[ci] / all[ci]) : 0.0f;
            if (ch[ci].explained[k] < explained_min) {
                ch[ci].explained[k] = 0;
                std::fill(Hw.begin() + (size_t)ci * B * C * K,Hw.begin() + (size_t)(ci + 1) * B * C * K,cplx(0,0));
            }
        }

        // into filters, faded in and out at the ends of the rates of change and the ends of the taps
        for (int ci=0;ci < C;ci++) {
            for (int b=0;b < B;b++) {
                for (int i=0;i < C;i++) {
                    const cplx *H = Hw.data() + (((size_t)ci * B + b) * C + i) * K;
                    bool any = false;

                    for (int kb=0;kb < K;kb++) {
                        if (H[kb] != cplx(0,0)) { any = true; break; }
                    }
                    if (!any)
                        continue;

                    memset(sspec,0,sizeof(AVComplexFloat) * (seglen / 2 + 1));
                    const int edge = std::max(1,K / 10);
                    for (int kb=0;kb < K;kb++) {
                        double t = 1;
                        if (kb < edge) t = 0.5 - 0.5 * cos((M_PI * (kb + 0.5)) / edge);
                        else if (kb >= K - edge) t = 0.5 - 0.5 * cos((M_PI * (K - kb - 0.5)) / edge);
                        sspec[kmin + kb].re = (float)(H[kb].real() * t);
                        sspec[kmin + kb].im = (float)(H[kb].imag() * t);
                    }
                    sinv_fn(sinv,sbuf,sspec,sizeof(AVComplexFloat));

                    float *h = ch[ci].h.data() + ((((size_t)k * B + b) * C + i) * seglen);
                    for (int n=0;n < seglen;n++) {
                        const int lag = n < seglen / 2 ? n : n - seglen;
                        h[n] = sbuf[n] * (float)(0.5 + 0.5 * cos((M_PI * lag) / (seglen / 2)));
                    }
                    ch[ci].linked[k] = 1;
                }
            }
        }
    }
}

void leveler_t::analyze(const float *const *x,int n) {
    samples_in += n;
    feed(x,n,false,NULL);
}

void leveler_t::end_analysis(void) {
    // the frames that the filter will have at the end, over the last of it
    feed(NULL,fft - hop + (hop - fill) % hop,false,NULL);

    total_frames = frames;
    find_filters();

    std::vector<block_sums_t>().swap(sums);
    for (auto &c : ch) {
        std::fill(c.in.begin(),c.in.end(),0.0f);
        std::vector<float>().swap(c.ering);
    }
    fill = 0;
    frames = samples_in = samples_out = 0;
}

void leveler_t::filter(const float *const *x,int n,std::vector<float> *out) {
    samples_in += n;
    feed(x,n,true,out);
}

void leveler_t::end_filter(std::vector<float> *out) {
    while (samples_out < samples_in)
        feed(NULL,hop,true,out);
}

double leveler_t::linked_seconds(int ci) const {
    int64_t n = 0;
    for (int k=0;k < nblocks;k++) {
        if (ch[ci].explained[k] > 0)
            n += std::min((int64_t)block_frames,total_frames - (int64_t)k * block_frames);
    }
    return ((double)n * hop) / rate;
}

double leveler_t::explained_at(int ci,double t) const {
    if (nblocks <= 0)
        return 0;

    const int k = std::max(0,std::min(nblocks - 1,(int)((t * rate) / ((double)block_frames * hop))));
    return ch[ci].explained[k];
}

double leveler_t::change_rms_db(int ci) const {
    return ch[ci].mcount > 0 ? sqrt(ch[ci].msum / ch[ci].mcount) : 0;
}

double leveler_t::change_max_db(int ci) const {
    return ch[ci].mmax;
}
