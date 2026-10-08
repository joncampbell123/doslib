/* vhsstabilize: measuring how far the picture moves up or down from one frame to another.
 *
 * Each line of the picture is reduced to its mean, in a few vertical strips side by side,
 * and those profiles are matched up and down against the profiles of another frame.
 * Moving the picture up or down moves the profile with it, while the horizontal jitter
 * of VHS hardly changes the mean of a line at all. */

#include <math.h>

#include <algorithm>

#include "motion.h"

void make_profile(profile_t &p,const plane_t &pl,unsigned int first,unsigned int step,unsigned int strips,unsigned int left,unsigned int right) {
    const unsigned int n = count_lines(pl.height,first,step);

    p.lines = n;
    p.strips = strips;
    p.first = first;
    p.step = step;
    p.v.assign((size_t)n * strips,0.0f);
    if (strips == 0 || left + right + strips > pl.width)
        return;

    const unsigned int x0 = left;
    const unsigned int xw = pl.width - left - right;
    const double scale = 1.0 / (double)pl.maxval();

    for (unsigned int j=0;j < n;j++) {
        const uint8_t *row = pl.line(first + (j * step));

        for (unsigned int s=0;s < strips;s++) {
            const unsigned int sx0 = x0 + (unsigned int)(((unsigned long long)xw * s) / strips);
            const unsigned int sx1 = x0 + (unsigned int)(((unsigned long long)xw * (s + 1u)) / strips);
            unsigned long long sum = 0;

            if (pl.bytes == 2) {
                const uint16_t *r16 = (const uint16_t*)row;
                for (unsigned int x=sx0;x < sx1;x++) sum += r16[x];
            }
            else {
                for (unsigned int x=sx0;x < sx1;x++) sum += row[x];
            }

            p.v[((size_t)s * n) + j] = (float)(((double)sum / (double)(sx1 - sx0)) * scale);
        }
    }
}

static inline int clampi(int v,int lo,int hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// Catmull-Rom weights for a point f (0 <= f < 1) of the way from the second of four samples to the third
static void cubic_weights(double w[4],double f) {
    const double f2 = f * f,f3 = f2 * f;

    w[0] = 0.5 * (-f3 + (2.0 * f2) - f);
    w[1] = 0.5 * ((3.0 * f3) - (5.0 * f2) + 2.0);
    w[2] = 0.5 * ((-3.0 * f3) + (4.0 * f2) + f);
    w[3] = 0.5 * (f3 - f2);
}

// the value of a strip of n samples at a point between them, the ends repeating past the ends
static double sample_at(const float *a,int n,double x) {
    const double fl = floor(x);
    const int i = (int)fl;
    const double f = x - fl;
    double w[4];

    if (f == 0.0)
        return a[clampi(i,0,n-1)];

    cubic_weights(w,f);
    return (w[0] * a[clampi(i-1,0,n-1)]) + (w[1] * a[clampi(i,0,n-1)]) +
           (w[2] * a[clampi(i+1,0,n-1)]) + (w[3] * a[clampi(i+2,0,n-1)]);
}

void merge_fields(profile_t &frame,const profile_t &f0,const profile_t &f1,double d,unsigned int height) {
    const unsigned int strips = f0.strips;

    frame.lines = height;
    frame.strips = strips;
    frame.first = 0;
    frame.step = 1;
    frame.v.assign((size_t)height * strips,0.0f);

    for (unsigned int s=0;s < strips;s++) {
        const float *a = f0.strip(s);
        const float *b = f1.strip(s);
        float *o = &frame.v[(size_t)s * height];

        // line 2i+1 of the frame, picture down d lines in field 1, is line i + d/2 of field 1
        for (unsigned int y=0;y < height;y++) {
            if ((y & 1u) == 0u)
                o[y] = a[std::min(y >> 1u,f0.lines - 1u)];
            else
                o[y] = (float)sample_at(b,(int)f1.lines,(double)(y >> 1u) + (d / 2.0));
        }
    }
}

// sums of the current picture's profile over the lines compared, which do not change with the shift
struct cur_sums_t {
    std::vector<double>         s,ss;
};

// how well cur lines [i0,i1), which are lines first + (step * i) of the frame,
// match the same lines less shift of ref, -1 to 1.
//
// This is the normalized cross-correlation of each strip, combined so that each strip counts
// as much as it has contrast: a strip that is all one color has nothing to match and adds nothing.
static double match_score(const profile_t &ref,const profile_t &cur,int i0,int i1,double shift,const cur_sums_t &cs) {
    const int n = (int)ref.lines;
    const double cnt = (double)(i1 - i0);
    const double fl = floor(-shift);
    const int io = (int)fl + (int)cur.first;
    const double f = -shift - fl;
    const int step = (int)cur.step;
    double num = 0,den = 0;
    double w[4];

    cubic_weights(w,f);

    for (unsigned int k=0;k < cur.strips;k++) {
        const float *a = cur.strip(k);
        const float *b = ref.strip(k);
        double sb = 0,sbb = 0,sab = 0;

        for (int i=i0;i < i1;i++) {
            const int y = (i * step) + io;
            double bv;

            if (f == 0.0)
                bv = b[clampi(y,0,n-1)];
            else
                bv = (w[0] * b[clampi(y-1,0,n-1)]) + (w[1] * b[clampi(y,0,n-1)]) +
                     (w[2] * b[clampi(y+1,0,n-1)]) + (w[3] * b[clampi(y+2,0,n-1)]);

            sb += bv;
            sbb += bv * bv;
            sab += a[i] * bv;
        }

        const double cov = sab - ((cs.s[k] * sb) / cnt);
        const double va = cs.ss[k] - ((cs.s[k] * cs.s[k]) / cnt);
        const double vb = sbb - ((sb * sb) / cnt);

        num += cov;
        if (va > 0.0 && vb > 0.0)
            den += sqrt(va * vb);
    }

    return den > 0.0 ? (num / den) : 0.0;
}

// the standard deviation of a profile over lines [i0,i1), all strips together
static double profile_contrast(const profile_t &p,int i0,int i1) {
    const double cnt = (double)(i1 - i0);
    double var = 0;

    for (unsigned int k=0;k < p.strips;k++) {
        const float *a = p.strip(k);
        double s = 0,ss = 0;

        for (int i=i0;i < i1;i++) {
            s += a[i];
            ss += (double)a[i] * a[i];
        }

        var += (ss - ((s * s) / cnt)) / cnt;
    }

    return sqrt(std::max(0.0,var / p.strips));
}

match_t match_profiles(const profile_t &ref,const profile_t &cur,const match_params_t &mp) {
    match_t r;

    if (ref.strips != cur.strips || cur.strips == 0 || cur.lines == 0 || ref.step != 1u || ref.lines <= cur.first + (cur.step * (cur.lines - 1u)))
        return r;

    // compare the lines of cur that, at any shift in range, stay inside the top and bottom margins of ref
    const int R = (int)mp.range;
    const int lo = (int)mp.top + R;
    const int hi = (int)ref.lines - (int)mp.bottom - R;
    const int step = (int)cur.step;
    const int i0 = std::max(0,(lo - (int)cur.first + step - 1) / step);
    const int i1 = std::min((int)cur.lines,std::max(0,(hi - (int)cur.first + step - 1) / step));
    if ((i1 - i0) < 8)
        return r;

    // a picture all one color (black, a blue screen) has nothing to match
    if (profile_contrast(cur,i0,i1) < mp.min_contrast || profile_contrast(ref,(int)mp.top,(int)ref.lines - (int)mp.bottom) < mp.min_contrast)
        return r;

    cur_sums_t cs;
    cs.s.assign(cur.strips,0.0);
    cs.ss.assign(cur.strips,0.0);
    for (unsigned int k=0;k < cur.strips;k++) {
        const float *a = cur.strip(k);
        for (int i=i0;i < i1;i++) {
            cs.s[k] += a[i];
            cs.ss[k] += (double)a[i] * a[i];
        }
    }

    // whole lines first
    std::vector<double> coarse((size_t)(R * 2) + 1u);
    int best = 0;

    for (int s=-R;s <= R;s++) {
        coarse[(size_t)(s + R)] = match_score(ref,cur,i0,i1,(double)s,cs);
        if (coarse[(size_t)(s + R)] > coarse[(size_t)(best + R)])
            best = s;
    }

    r.shift = best;
    r.score = coarse[(size_t)(best + R)];

    // at the end of the range, the picture may well have moved further than the range
    if (best == -R || best == R)
        return r;

    // then eighths of a line from the line before to the line after the best
    double fine[17];
    int kb = 8;

    fine[8] = r.score;
    for (int k=-8;k <= 8;k++) {
        if (k == 0)
            continue;
        else if (k == -8 || k == 8)
            fine[k + 8] = coarse[(size_t)(best + (k / 8) + R)];
        else
            fine[k + 8] = match_score(ref,cur,i0,i1,best + (k / 8.0),cs);

        if (fine[k + 8] > fine[kb])
            kb = k + 8;
    }

    // and the peak of a parabola through the best eighth and the eighths on each side of it
    double off = 0;
    if (kb > 0 && kb < 16) {
        const double d = fine[kb - 1] - (2.0 * fine[kb]) + fine[kb + 1];
        if (d < 0.0)
            off = std::max(-0.5,std::min(0.5,(0.5 * (fine[kb - 1] - fine[kb + 1])) / d));
    }

    r.shift = best + (((kb - 8) + off) / 8.0);
    r.score = fine[kb];
    r.ok = r.score >= mp.min_score;
    return r;
}

match_t match_fields(const profile_t &ref,const profile_t &cur,const match_params_t &mp) {
    const unsigned int step = cur.step;
    profile_t r = ref,c = cur;
    match_params_t fp = mp;

    // as if each field were a frame of its own
    r.first = c.first = 0;
    r.step = c.step = 1;
    fp.top = (mp.top + step - 1u) / step;
    fp.bottom = (mp.bottom + step - 1u) / step;
    fp.range = (mp.range + step - 1u) / step;

    match_t m = match_profiles(r,c,fp);
    m.shift *= step;
    return m;
}

// true if the fields of a frame, with field 1 d lines below field 0, are more or less
// an odd number of lines apart: then both are lines of the picture of the same parity,
// and the lines in between have to be made up to make the whole frame
static bool fields_odd(double d) {
    const double r = d - (2.0 * floor(d / 2.0));
    return r > 0.5 && r < 1.5;
}

void tracker_t::restart_at(const profile_t cur[],const double pos[]) {
    make_ref(key,cur,pos);
    prev = key;
    key_is_prev = true;
    started = true;
}

void tracker_t::make_ref(ref_t &r,const profile_t cur[],const double pos[]) const {
    if (fields == 2u)
        merge_fields(r.frame,cur[0],cur[1],pos[1] - pos[0],height);
    else
        r.frame = cur[0];

    r.pos[0] = pos[0];
    r.pos[1] = fields == 2u ? pos[1] : pos[0];
}

void tracker_t::add(const profile_t cur[],step_t st[]) {
    double pos[2] = {0,0};

    if (!started) {
        restart_at(cur,pos);

        for (unsigned int p=0;p < fields;p++) {
            st[p].pos = 0;
            st[p].score = 1;
            st[p].how = FIRST;
        }
        return;
    }

    bool new_key = false;

    for (unsigned int p=0;p < fields;p++) {
        const match_t mp = match_profiles(prev.frame,cur[p],params);
        const match_t mk = key_is_prev ? mp : match_profiles(key.frame,cur[p],params);

        // the key frame, unless the frame before matches noticeably better
        if (mk.ok && (key_is_prev || !mp.ok || mk.score >= (mp.score - 0.02))) {
            st[p].pos = key.pos[0] + mk.shift;
            st[p].score = mk.score;
            st[p].how = key_is_prev ? PREV : KEY;
            if (mk.score < key_score)
                new_key = true;
        }
        else if (mp.ok) {
            st[p].pos = prev.pos[0] + mp.shift;
            st[p].score = mp.score;
            st[p].how = PREV;
            new_key = true;
        }
        else {
            st[p].pos = prev.pos[p];
            st[p].score = std::max(mp.score,mk.score);
            st[p].how = LOST;
            new_key = true;
        }
    }

    // the content of both fields moves together, so a field that could not be matched
    // stays where it was from the other field, as it was in the frame before
    if (fields == 2u) {
        for (unsigned int p=0;p < 2u;p++) {
            const unsigned int o = p ^ 1u;
            if (st[p].how == LOST && st[o].how != LOST)
                st[p].pos = st[o].pos + (prev.pos[p] - prev.pos[o]);
        }
    }

    for (unsigned int p=0;p < fields;p++)
        pos[p] = st[p].pos;

    // a key frame made from fields an odd number of lines apart is not the whole picture,
    // so replace it with the first frame that is
    if (fields == 2u && fields_odd(key.pos[1] - key.pos[0]) && !fields_odd(pos[1] - pos[0]))
        new_key = true;

    make_ref(prev,cur,pos);
    if (new_key)
        key = prev;
    key_is_prev = new_key;
}

std::vector<double> smooth_path(const std::vector<double> &path,double radius,bool robust) {
    const size_t n = path.size();
    std::vector<double> out(n),trust(n,1.0);
    const int r = (int)ceil(radius);

    if (r <= 0)
        return path;

    const double sigma = radius / 2.0;
    std::vector<double> w((size_t)(r * 2) + 1u);
    for (int k=-r;k <= r;k++)
        w[(size_t)(k + r)] = exp(-((double)k * k) / (2.0 * sigma * sigma));

    // Jitter jumps far from the path for a few fields, which would pull a plain average
    // toward it. So after each round, the points far from the path count for less in the
    // next round, by how far from it they are (Huber weights).
    for (unsigned int round=0;round < (robust ? 4u : 1u);round++) {
        // at the ends of the path, the weights of what there is
        for (size_t i=0;i < n;i++) {
            double sw = 0,sv = 0;

            for (int k=-r;k <= r;k++) {
                const long long j = (long long)i + k;
                if (j >= 0 && j < (long long)n) {
                    const double wt = w[(size_t)(k + r)] * trust[(size_t)j];
                    sw += wt;
                    sv += wt * path[(size_t)j];
                }
            }

            out[i] = sv / sw;
        }

        for (size_t i=0;i < n;i++) {
            const double far = fabs(path[i] - out[i]);
            trust[i] = far > 0.5 ? (0.5 / far) : 1.0;
        }
    }

    return out;
}

std::vector<double> median_path(const std::vector<double> &path,unsigned int radius) {
    const size_t n = path.size();
    std::vector<double> out(n),win;

    for (size_t i=0;i < n;i++) {
        const size_t a = i >= radius ? (i - radius) : 0u;
        const size_t b = std::min(n,i + radius + 1u);

        win.assign(path.begin() + (ptrdiff_t)a,path.begin() + (ptrdiff_t)b);
        std::nth_element(win.begin(),win.begin() + (ptrdiff_t)(win.size() / 2u),win.end());
        out[i] = win[win.size() / 2u];
    }

    return out;
}
