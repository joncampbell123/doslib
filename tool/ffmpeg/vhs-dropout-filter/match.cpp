#include <math.h>
#include <string.h>

#include <algorithm>
#include <complex>
#include <vector>

#include "match.h"

// a thumbnail with less standard deviation than this, in levels, is blank: the screen of a VCR with
// no tape playing, or after the end of it, which is the same as any other
static const float BLANK = 4.0f;

// frames of the pattern of change added together, to find where in the donor a window of the input is
// quickly, to within this many frames, before the thumbnails find it to the frame
static const int DECIMATE = 4;

// how far either way the thumbnails look for each frame, from where the window it is in was found,
// for VCRs that do not play at quite the same speed
static const int FRAME_SEARCH = 3;

// each thumbnail made zero mean and unit variance, in 1/32 units in a byte, which is plenty to tell one
// frame from the next, and a quarter of the memory of hours of them as floats; and how much the picture
// changes from the frame before
static const int SIG_SCALE = 32;

struct sig_t {
    size_t                      n = 0;
    std::vector<int8_t>         f;
    std::vector<float>          sd;
    std::vector<float>          act;

    const int8_t *at(size_t i) const {
        return &f[i * THUMB_SIZE];
    }
    bool blank(size_t i) const {
        return sd[i] < BLANK;
    }
};

static void make_sig(sig_t &s,const thumbs_t &t) {
    s.n = t.count();
    s.f.resize(s.n * THUMB_SIZE);
    s.sd.resize(s.n);
    s.act.assign(s.n,0.0f);

    for (size_t i=0;i < s.n;i++) {
        const uint8_t *p = t.thumb(i);
        double sum = 0,sum2 = 0;
        for (int k=0;k < THUMB_SIZE;k++) {
            sum += p[k];
            sum2 += (double)p[k] * p[k];
        }
        const double mean = sum / THUMB_SIZE;
        const double sd = sqrt(std::max(0.0,(sum2 / THUMB_SIZE) - (mean * mean)));
        const float inv = (float)(SIG_SCALE / std::max(sd,1.0));
        int8_t *f = &s.f[i * THUMB_SIZE];
        for (int k=0;k < THUMB_SIZE;k++)
            f[k] = (int8_t)std::min(127.0f,std::max(-127.0f,floorf(((float)(p[k] - mean) * inv) + 0.5f)));
        s.sd[i] = (float)sd;
    }

    // how much each frame changes from the one before, with the slow changes taken out, so that
    // it is the pattern of change that is matched, not how much there is
    std::vector<float> d(s.n,0.0f);
    for (size_t i=1;i < s.n;i++) {
        if (s.blank(i) || s.blank(i - 1u))
            continue;
        const int8_t *a = s.at(i),*b = s.at(i - 1u);
        int sum = 0;
        for (int k=0;k < THUMB_SIZE;k++)
            sum += abs((int)a[k] - (int)b[k]);
        d[i] = (float)sum / (float)(THUMB_SIZE * SIG_SCALE);
    }
    const int r = 30;
    double run = 0;
    size_t cnt = 0;
    for (size_t i=0;i < std::min(s.n,(size_t)r);i++) { run += d[i]; cnt++; }
    for (size_t i=0;i < s.n;i++) {
        if (i + r < s.n) { run += d[i + r]; cnt++; }
        if (i > (size_t)r) { run -= d[i - r - 1u]; cnt--; }
        if (!s.blank(i))
            s.act[i] = d[i] - (float)(run / (double)cnt);
    }
}

// how alike frame i of a and frame j of b are, -1 to 1
static float frame_score(const sig_t &a,size_t i,const sig_t &b,size_t j) {
    const int8_t *x = a.at(i),*y = b.at(j);
    int sum = 0;
    for (int k=0;k < THUMB_SIZE;k++)
        sum += (int)x[k] * (int)y[k];
    return (float)sum / (float)(THUMB_SIZE * SIG_SCALE * SIG_SCALE);
}

/////////////////////////////////////////////////////////////////////////////

typedef std::complex<double> cplx;

static void fft(std::vector<cplx> &a,bool inverse) {
    const size_t n = a.size();

    for (size_t i=1,j=0;i < n;i++) {
        size_t bit = n >> 1u;
        for (;j & bit;bit >>= 1u)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(a[i],a[j]);
    }
    for (size_t len=2;len <= n;len <<= 1u) {
        const double ang = (2.0 * M_PI / (double)len) * (inverse ? 1.0 : -1.0);
        const cplx wl(cos(ang),sin(ang));
        for (size_t i=0;i < n;i += len) {
            cplx w(1.0,0.0);
            for (size_t j=0;j < len / 2u;j++) {
                const cplx u = a[i + j],v = a[i + j + (len / 2u)] * w;
                a[i + j] = u + v;
                a[i + j + (len / 2u)] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) {
        for (size_t i=0;i < n;i++)
            a[i] /= (double)n;
    }
}

// the pattern of change of the whole donor, ready to find windows of the input in it
class finder_t {
public:
    finder_t(const sig_t &d,size_t win) {
        m = d.n / DECIMATE;
        sig.assign(m,0.0);
        for (size_t i=0;i < m;i++) {
            for (int k=0;k < DECIMATE;k++)
                sig[i] += d.act[(i * DECIMATE) + k];
        }
        wn = std::max((size_t)1,win / DECIMATE);
        size = 1;
        while (size < m + wn)
            size <<= 1u;
        spec.assign(size,cplx(0,0));
        for (size_t i=0;i < m;i++)
            spec[i] = cplx(sig[i],0);
        fft(spec,false);

        // the sums of the donor over each stretch as long as a window, to normalize by
        cs.assign(m + 1u,0.0);
        cs2.assign(m + 1u,0.0);
        for (size_t i=0;i < m;i++) {
            cs[i + 1u] = cs[i] + sig[i];
            cs2[i + 1u] = cs2[i] + (sig[i] * sig[i]);
        }
    }

    // the frames of the donor where the window of the input that starts at frame s is most likely to be,
    // the best first
    std::vector<int64_t> find(const sig_t &in,size_t s,size_t count) const {
        std::vector<int64_t> r;
        if (m < wn + 1u)
            return r;

        std::vector<double> w(wn,0.0);
        double mean = 0,var = 0;
        for (size_t i=0;i < wn;i++) {
            for (int k=0;k < DECIMATE;k++) {
                const size_t f = s + (i * DECIMATE) + k;
                if (f < in.n)
                    w[i] += in.act[f];
            }
            mean += w[i];
        }
        mean /= (double)wn;
        for (size_t i=0;i < wn;i++) {
            w[i] -= mean;
            var += w[i] * w[i];
        }
        if (var <= 1e-12)
            return r;

        std::vector<cplx> a(size,cplx(0,0));
        for (size_t i=0;i < wn;i++)
            a[i] = cplx(w[i],0);
        fft(a,false);
        for (size_t i=0;i < size;i++)
            a[i] = spec[i] * std::conj(a[i]);
        fft(a,true);

        const size_t lags = m - wn + 1u;
        std::vector<std::pair<double,size_t> > c(lags);
        for (size_t l=0;l < lags;l++) {
            const double mu = (cs[l + wn] - cs[l]) / (double)wn;
            const double v = std::max(((cs2[l + wn] - cs2[l]) / (double)wn) - (mu * mu),1e-12);
            c[l] = std::make_pair(a[l].real() / sqrt(v * (double)wn * var),l);
        }
        const size_t k = std::min(count * 4u,lags);
        std::partial_sort(c.begin(),c.begin() + (ptrdiff_t)k,c.end(),[](const std::pair<double,size_t> &x,const std::pair<double,size_t> &y) { return x.first > y.first; });
        for (size_t i=0;i < k && r.size() < count;i++) {
            const int64_t f = (int64_t)(c[i].second * DECIMATE);
            bool near = false;
            for (int64_t q : r)
                near = near || llabs(q - f) <= 2 * DECIMATE;
            if (!near)
                r.push_back(f);
        }
        return r;
    }
private:
    size_t                      m = 0,wn = 0,size = 0;
    std::vector<double>         sig,cs,cs2;
    std::vector<cplx>           spec;
};

/////////////////////////////////////////////////////////////////////////////

void match_frames(std::vector<frame_match_t> &out,const thumbs_t &in_t,const thumbs_t &donor_t,const match_params_t &mp) {
    sig_t in,d;

    out.assign(in_t.count(),frame_match_t());
    make_sig(in,in_t);
    make_sig(d,donor_t);
    if (in.n == 0 || d.n == 0)
        return;

    const double verify = std::max(0.5,mp.min_score - 0.05);

    // whether input frame i may be donor frame j: not blank, and not itself in the same file
    auto allowed = [&](size_t i,int64_t j) -> bool {
        if (j < 0 || (size_t)j >= d.n || in.blank(i) || d.blank((size_t)j))
            return false;
        return mp.min_gap <= 0 || fabs(in_t.time[i] - donor_t.time[(size_t)j]) >= mp.min_gap;
    };

    // how well the window of W frames starting at s matches the donor that many frames further on,
    // from every 5th frame of it, and the best of a few frames either way of that
    size_t W = 0;
    auto window_score = [&](size_t s,int64_t lag,int search,int64_t &best_lag) -> double {
        double best = -1;
        for (int64_t l=lag - search;l <= lag + search;l++) {
            double sum = 0;
            size_t cnt = 0,tried = 0;
            for (size_t i=s;i < std::min(s + W,in.n);i += (W >= 50u ? 5u : 2u)) {
                tried++;
                const int64_t j = (int64_t)i + l;
                if (!allowed(i,j))
                    continue;
                sum += frame_score(in,i,d,(size_t)j);
                cnt++;
            }
            if (cnt * 2u < tried || cnt == 0)
                continue;
            if (sum / (double)cnt > best) {
                best = sum / (double)cnt;
                best_lag = l;
            }
        }
        return best;
    };

    // Where each window of the input is in the donor, if it is: where the window before it was, if that
    // still matches, which it does most of the time, or else where the pattern of change says it may be.
    // In long windows, which are found most surely, and in windows short enough that a part of an edited
    // copy between two cuts is the whole of some of them. Each frame then takes the best of them.
    struct found_t {
        size_t                  start,len;
        int64_t                 lag;
    };
    std::vector<found_t> found,missed;
    const size_t W1 = std::min((size_t)mp.window,in.n);
    const size_t W2 = std::max((size_t)DECIMATE * 4u,W1 / 3u);
    for (unsigned int pass=0;pass < 2u;pass++) {
        W = pass == 0u ? W1 : W2;
        if (W < (size_t)DECIMATE * 2u || (pass == 1u && W2 >= W1))
            continue;

        const size_t hop = std::max((size_t)1,W / 2u);
        finder_t finder(d,W);
        bool have = false;
        int64_t last = 0;
        for (size_t s=0;s < in.n;s += hop) {
            const size_t e = std::min(s + W,in.n);
            {
                int64_t lag = 0;
                double sc = -1;

                if (have)
                    sc = window_score(s,last,FRAME_SEARCH,lag);
                if (sc < verify) {
                    sc = -1;
                    for (int64_t f : finder.find(in,s,32)) {
                        int64_t l2 = 0;
                        // as far either way as the candidates are kept apart, so that none is left out between them
                        const double c = window_score(s,f - (int64_t)s,2 * DECIMATE,l2);
                        if (c > sc) {
                            sc = c;
                            lag = l2;
                        }
                    }
                }

                have = sc >= verify;
                if (have) {
                    last = lag;
                    found.push_back(found_t{s,e - s,lag});
                }
                else {
                    missed.push_back(found_t{s,e - s,0});
                }
            }
            if (s + W >= in.n)
                break;
        }
    }
    // A window that was not found, where those near it were: the pattern of change of a short window is
    // like that of many places in hours of video. The tape goes on as it did, so where the windows near it
    // are, a frame for each frame, is where it is likely to be too.
    {
        const std::vector<found_t> got = found;
        for (const found_t &m : missed) {
            W = m.len;
            std::vector<int64_t> tried;
            for (const found_t &f : got) {
                if (f.start + f.len + (3u * m.len) < m.start || m.start + m.len + (3u * m.len) < f.start)
                    continue;
                if (std::find(tried.begin(),tried.end(),f.lag) != tried.end())
                    continue;
                tried.push_back(f.lag);
                int64_t lag = 0;
                if (window_score(m.start,f.lag,FRAME_SEARCH,lag) >= verify) {
                    found.push_back(found_t{m.start,m.len,lag});
                    break;
                }
            }
        }
    }
    std::sort(found.begin(),found.end(),[](const found_t &a,const found_t &b) { return a.start < b.start; });

    // then each frame, from where the windows it is in were found, a few frames either way
    std::vector<int64_t> lag(in.n,INT64_MIN);
    std::vector<float> score(in.n,-1.0f);
    for (size_t i=0;i < in.n;i++) {
        // a little for each frame away from where the window was found, for frames much like the
        // frames either side of them, as in a slow pan, which would go to any of them
        float best = -2.0f;
        for (size_t k=0;k < found.size() && found[k].start <= i;k++) {
            if (found[k].start + found[k].len <= i)
                continue;
            for (int64_t l=found[k].lag - FRAME_SEARCH;l <= found[k].lag + FRAME_SEARCH;l++) {
                const int64_t j = (int64_t)i + l;
                if (!allowed(i,j))
                    continue;
                const float c = frame_score(in,i,d,(size_t)j);
                const float cp = c - (0.0005f * (float)llabs(l - found[k].lag));
                if (cp > best) {
                    best = cp;
                    score[i] = c;
                    lag[i] = l;
                }
            }
        }
    }

    // The donor goes on a frame for each frame of the input. Where frames of the input are not in it, as
    // where an edited copy left them out, each goes to whichever frame of the donor is most like it, which
    // is the same few frames for all of them, so the frames around it are not the frames after and before.
    // A frame is kept only where most of the frames near it are as far from theirs in the donor as it is.
    auto steady = [&](std::vector<int64_t> &lg) {
        std::vector<int64_t> k2 = lg;
        for (size_t i=0;i < in.n;i++) {
            if (lg[i] == INT64_MIN)
                continue;
            unsigned int agree = 0,near = 0;
            for (size_t k=(i >= 3u ? i - 3u : 0u);k <= std::min(in.n - 1u,i + 3u);k++) {
                if (k == i)
                    continue;
                near++;
                if (lg[k] == lg[i])
                    agree++;
            }
            if (agree * 2u < near)
                k2[i] = INT64_MIN;
        }
        lg.swap(k2);
    };
    steady(lag);

    // A frame much like the frames around it can match a frame or two either way as well as its own.
    // The donor goes on as the input does, so each frame takes the median of the frames around it,
    // unless that does not match it, as at a cut in an edited copy.
    for (size_t i=0;i < in.n;i++) {
        std::vector<int64_t> near;
        for (size_t k=(i >= 7u ? i - 7u : 0u);k <= std::min(in.n - 1u,i + 7u);k++) {
            if (lag[k] != INT64_MIN)
                near.push_back(lag[k]);
        }
        int64_t l = lag[i];
        float c = l != INT64_MIN ? score[i] : -1.0f;
        if (!near.empty()) {
            std::nth_element(near.begin(),near.begin() + (ptrdiff_t)(near.size() / 2u),near.end());
            const int64_t m = near[near.size() / 2u];
            const int64_t j = (int64_t)i + m;
            if (allowed(i,j)) {
                const float cm = frame_score(in,i,d,(size_t)j);
                if (lag[i] == INT64_MIN || cm >= score[i] - 0.02f) {
                    l = m;
                    c = cm;
                }
            }
        }
        // and it is the best of the frames of the donor either side of it: a frame that is not in the
        // donor is more like the frames of the donor nearer to it in time, on one side or the other
        if (l != INT64_MIN && c >= mp.min_score) {
            bool peak = true;
            for (int64_t e=-2;e <= 2 && peak;e++) {
                const int64_t j = (int64_t)i + l + e;
                if (e != 0 && allowed(i,j) && frame_score(in,i,d,(size_t)j) > c + 0.002f)
                    peak = false;
            }
            if (peak) {
                out[i].frame = (int64_t)i + l;
                out[i].score = c;
            }
        }
    }

    // and again, of what is left
    std::vector<int64_t> fl(in.n,INT64_MIN);
    for (size_t i=0;i < in.n;i++) {
        if (out[i].frame >= 0)
            fl[i] = out[i].frame - (int64_t)i;
    }
    steady(fl);
    for (size_t i=0;i < in.n;i++) {
        if (fl[i] == INT64_MIN)
            out[i] = frame_match_t();
    }

    // and a frame between two that go on the same, that goes on with them as well, as one with a dropout
    // big enough to make it less like its own frame of the donor than the one next to it
    for (size_t i=1;i + 1u < in.n;i++) {
        if (out[i].frame >= 0 || out[i - 1u].frame < 0 || out[i + 1u].frame < 0)
            continue;
        const int64_t l = out[i - 1u].frame - (int64_t)(i - 1u);
        const int64_t j = (int64_t)i + l;
        if (out[i + 1u].frame - (int64_t)(i + 1u) != l || !allowed(i,j))
            continue;
        const float c = frame_score(in,i,d,(size_t)j);
        if (c >= mp.min_score) {
            out[i].frame = j;
            out[i].score = c;
        }
    }
}
