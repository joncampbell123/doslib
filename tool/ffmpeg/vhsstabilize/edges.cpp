/* vhsstabilize: finding the left and right edges of the picture on each line, and how far
 * each line has to move left or right to straighten them.
 *
 * A capture of a VHS tape has a little of the blanking at each side of the picture, which is
 * black, and the picture starts and ends against it at the same place on every line if the
 * timing of the lines is right. Without a time base corrector it is not quite right: each line
 * starts a little early or late, which bends what should be straight up and down. The picture
 * on a line moves as far as its edges do, so how far they are from where they are most of the
 * time is how far to move the line back. The two edges together also say how much longer or
 * shorter the line is than it should be. */

#include <stdlib.h>
#include <math.h>

#include <algorithm>
#include <utility>

#include "edges.h"
#include "motion.h"

static double median_of(std::vector<double> v) {
    if (v.empty())
        return NAN;

    std::nth_element(v.begin(),v.begin() + (ptrdiff_t)(v.size() / 2u),v.end());
    return v[v.size() / 2u];
}

// fill in the values that are NaN, on a straight line between the values on each side of them,
// or the same as the nearest value past the first or the last. How many were not NaN.
static size_t fill_gaps(std::vector<double> &v) {
    const size_t n = v.size();
    size_t known = 0,last = 0;

    for (size_t i=0;i < n;i++) {
        if (isnan(v[i]))
            continue;

        if (known == 0u) {
            for (size_t k=0;k < i;k++)
                v[k] = v[i];
        }
        else {
            for (size_t k=last + 1u;k < i;k++)
                v[k] = v[last] + (((v[i] - v[last]) * (double)(k - last)) / (double)(i - last));
        }

        last = i;
        known++;
    }
    if (known != 0u) {
        for (size_t k=last + 1u;k < n;k++)
            v[k] = v[last];
    }

    return known;
}

// n samples of one side of a line, from the edge of the frame inward
template <typename T> static void side_samples(float *o,const plane_t &pl,unsigned int y,unsigned int side,unsigned int n) {
    const T *r = (const T*)pl.line(y);
    const float scale = 1.0f / (float)pl.maxval();

    if (side == 0u) {
        for (unsigned int i=0;i < n;i++) o[i] = (float)r[i] * scale;
    }
    else {
        for (unsigned int i=0;i < n;i++) o[i] = (float)r[pl.width - 1u - i] * scale;
    }
}

// how much v rises around x, about the height of a step there: a derivative, smoothed
static inline float rise(const float *v,int x) {
    return ((2.0f * (v[x + 1] - v[x - 1])) + (v[x + 2] - v[x - 2])) / 3.0f;
}

// where the picture starts in v, n samples from the edge of the frame inward, to a fraction of
// a sample, or NaN: the first place from the outside, from 'from' to 'to', where v rises from the
// blanking, which is all that is outside it, to the picture. 'step' is how far it rises.
static double edge_in(const float *v,int n,int from,int to,const edge_params_t &ep,float &step) {
    int x = std::max(from,2);

    to = std::min(to,n - 5);
    while (x <= to) {
        if (rise(v,x) < (float)(ep.min_step * 0.5)) {
            x++;
            continue;
        }

        // the steepest part of the rise
        while (x < to && rise(v,x + 1) >= rise(v,x))
            x++;

        // everything outside it is as dark as the blanking: past anything brighter, there is no edge to find
        float most = 0;
        for (int i=0;i <= (x - 3);i++)
            most = std::max(most,v[i]);
        if (most > (float)ep.blank_max)
            return NAN;

        // and the picture just inside it is brighter than just outside it by enough
        const int l0 = std::max(0,x - 4);
        float lo = 0;
        for (int i=l0;i <= (x - 2);i++)
            lo += v[i];
        lo /= (float)(x - 1 - l0);
        const float hi = (v[x + 2] + v[x + 3] + v[x + 4]) / 3.0f;

        if ((hi - lo) >= (float)ep.min_step && lo <= (float)ep.blank_max) {
            // halfway up, nearest the steepest part
            const float t = (lo + hi) * 0.5f;
            step = hi - lo;
            double pos = (double)x,near = 1e9;

            for (int i=x - 2;i < (x + 2);i++) {
                if (v[i] < t && v[i + 1] >= t) {
                    const double p = (double)i + ((double)(t - v[i]) / (double)(v[i + 1] - v[i]));
                    if (fabs(p - (double)x) < near) {
                        near = fabs(p - (double)x);
                        pos = p;
                    }
                }
            }

            return pos;
        }

        x++;
    }

    return NAN;
}

void find_edges(line_edges_t &e,const plane_t &pl,unsigned int side,double expect,const edge_params_t &ep) {
    const int n = (int)std::min(ep.search[side],pl.width / 2u);
    const double last = (double)(pl.width - 1u);
    int from = 2,to = n - 5;

    e.pos.assign(pl.height,NAN);
    e.step.assign(pl.height,0.0);
    if (n < 8)
        return;

    // where to look, in samples from the edge of the frame
    if (!isnan(expect)) {
        const double at = side == 0u ? expect : (last - expect);
        from = (int)floor(at - ep.max_shift - 1.0);
        to = (int)ceil(at + ep.max_shift + 1.0);
    }

    std::vector<float> v((size_t)n);
    for (unsigned int y=0;y < pl.height;y++) {
        if (pl.bytes == 2)
            side_samples<uint16_t>(&v[0],pl,y,side,(unsigned int)n);
        else
            side_samples<uint8_t>(&v[0],pl,y,side,(unsigned int)n);

        float step = 0;
        const double p = edge_in(&v[0],n,from,to,ep,step);
        if (!isnan(p)) {
            e.pos[y] = side == 0u ? p : (last - p);
            e.step[y] = step;
        }
    }
}

double frame_edge(const line_edges_t &e,const edge_params_t &ep) {
    const std::vector<double> &pos = e.pos;
    const unsigned int h = (unsigned int)pos.size();
    const unsigned int y0 = std::min(ep.top,h);
    const unsigned int y1 = std::max(y0,h > ep.bottom ? (h - ep.bottom) : 0u);
    std::vector<double> found;

    for (unsigned int y=y0;y < y1;y++) {
        if (!isnan(pos[y]))
            found.push_back(pos[y]);
    }
    if (found.empty())
        return NAN;

    // it is the edge if at least a quarter of the lines have it there,
    // not something in the picture that a few lines have
    const double m = median_of(found);
    unsigned int near = 0;
    for (size_t i=0;i < found.size();i++) {
        if (fabs(found[i] - m) <= ep.max_shift)
            near++;
    }

    return (near * 4u) >= (y1 - y0) ? m : NAN;
}

bool edge_reference(std::vector<double> &ref,const std::vector<double> &found,double radius) {
    const size_t n = found.size();
    std::vector<double> known;

    for (size_t i=0;i < n;i++) {
        if (!isnan(found[i]))
            known.push_back(found[i]);
    }

    // in too few frames to be the edge of the picture
    if (known.empty() || (known.size() * 10u) < n) {
        ref.assign(n,NAN);
        return false;
    }

    if (radius <= 0) {
        ref.assign(n,median_of(known));
        return true;
    }

    ref = found;
    fill_gaps(ref);
    ref = median_path(ref,std::max(2u,(unsigned int)ceil(radius)));
    ref = smooth_path(ref,radius);
    return true;
}

// lines further than 'jump' from the median of the lines around them are taken to be wrong
static void reject_jumps(std::vector<double> &d,double jump) {
    const int n = (int)d.size();
    std::vector<bool> bad((size_t)n,false);
    std::vector<double> win;

    for (int j=0;j < n;j++) {
        if (isnan(d[(size_t)j]))
            continue;

        win.clear();
        for (int k=std::max(0,j - 3);k <= std::min(n - 1,j + 3);k++) {
            if (!isnan(d[(size_t)k]))
                win.push_back(d[(size_t)k]);
        }
        if (win.size() >= 3u && fabs(d[(size_t)j] - median_of(win)) > jump)
            bad[(size_t)j] = true;
    }

    for (int j=0;j < n;j++) {
        if (bad[(size_t)j])
            d[(size_t)j] = NAN;
    }
}

// the weighted median of the values that are not NaN, or NaN if there are none
static double weighted_median(const std::vector<double> &v,const std::vector<double> &w) {
    std::vector<std::pair<double,double> > vw;
    double total = 0,sum = 0;

    for (size_t i=0;i < v.size();i++) {
        if (!isnan(v[i]) && w[i] > 0.0) {
            vw.push_back(std::make_pair(v[i],w[i]));
            total += w[i];
        }
    }

    std::sort(vw.begin(),vw.end());
    for (size_t i=0;i < vw.size();i++) {
        sum += vw[i].second;
        if (sum >= (total / 2.0))
            return vw[i].first;
    }

    return NAN;
}

// the mean of the values of d at the nearest 3 lines from 'from' on, going by 'dir' (1 or -1), no further
// than 12 lines, that are among the lines 'use' says to and between lo and hi. NaN if there are none.
static double near_mean(const std::vector<double> &d,const std::vector<char> &use,int from,int dir,int lo,int hi) {
    double sum = 0;
    unsigned int cnt = 0;

    for (int j=from;j >= lo && j <= hi && abs(j - from) <= 12 && cnt < 3u;j += dir) {
        if (use[(size_t)j] && !isnan(d[(size_t)j])) {
            sum += d[(size_t)j];
            cnt++;
        }
    }

    return cnt != 0u ? (sum / (double)cnt) : NAN;
}

// Where the edges of a line are further apart or closer together than on most lines of the field, by
// more than 'apart', something in the picture next to one of them has moved it. Over each run of such
// lines, that is the edge that jumps where the run starts and ends, from the lines on either side where
// the edges agree, while the other goes on from them smoothly. Or the weaker edge, if there are no lines
// where they agree to go by.
static void settle_disagreements(std::vector<double> d[2],const std::vector<double> wt[2],double apart) {
    const int n = (int)d[0].size();
    std::vector<char> agree((size_t)n,0),differ((size_t)n,0);
    std::vector<double> sv;

    for (int j=0;j < n;j++) {
        if (!isnan(d[0][(size_t)j]) && !isnan(d[1][(size_t)j]))
            sv.push_back(d[1][(size_t)j] - d[0][(size_t)j]);
    }
    if (sv.empty())
        return;

    const double sm = median_of(sv);
    for (int j=0;j < n;j++) {
        if (!isnan(d[0][(size_t)j]) && !isnan(d[1][(size_t)j])) {
            if (fabs((d[1][(size_t)j] - d[0][(size_t)j]) - sm) > apart)
                differ[(size_t)j] = 1;
            else
                agree[(size_t)j] = 1;
        }
    }

    for (int j=0;j < n;) {
        if (!differ[(size_t)j]) {
            j++;
            continue;
        }

        // the run, up to the next line where they agree
        const int a = j;
        int b = j;
        for (int i=j + 1;i < n && !agree[(size_t)i];i++) {
            if (differ[(size_t)i])
                b = i;
        }
        j = b + 1;

        double jumps[2] = {0,0},weight[2] = {0,0};
        bool sides = false;
        for (unsigned int k=0;k < 2u;k++) {
            const double before = near_mean(d[k],agree,a - 1,-1,0,n - 1);
            const double after = near_mean(d[k],agree,b + 1,1,0,n - 1);

            if (!isnan(before)) {
                jumps[k] += fabs(near_mean(d[k],differ,a,1,a,b) - before);
                sides = true;
            }
            if (!isnan(after)) {
                jumps[k] += fabs(near_mean(d[k],differ,b,-1,a,b) - after);
                sides = true;
            }
            for (int i=a;i <= b;i++) {
                if (differ[(size_t)i])
                    weight[k] += wt[k][(size_t)i];
            }
        }

        const unsigned int wrong = sides ? (jumps[0] > jumps[1] ? 0u : 1u) : (weight[0] < weight[1] ? 0u : 1u);
        for (int i=a;i <= b;i++)
            d[wrong][(size_t)i] = NAN;
    }
}

static size_t count_known(const std::vector<double> &v) {
    size_t c = 0;

    for (size_t i=0;i < v.size();i++) {
        if (!isnan(v[i]))
            c++;
    }

    return c;
}

void line_warps(std::vector<hwarp_t> &warp,const line_edges_t edges[2],const double ref[2],unsigned int height,unsigned int fields,const edge_params_t &ep) {
    const double H = ep.max_shift;
    const double jump = std::max(1.0,H / 8.0);      // the most a line can be off from the lines around it
    const double apart = std::max(0.5,H / 16.0);    // the most the edges of a line can be off from each other
    bool use[2];

    warp.assign(height,hwarp_t());
    for (unsigned int k=0;k < 2u;k++)
        use[k] = !isnan(ref[k]) && edges[k].pos.size() == height && edges[k].step.size() == height;
    if (!use[0] && !use[1])
        return;

    // the picture is a line longer or shorter by the difference between how far its edges moved
    const double width = ref[1] - ref[0];
    const bool both = use[0] && use[1] && width >= 16.0;

    for (unsigned int p=0;p < fields;p++) {
        const unsigned int n = count_lines(height,p,fields);
        std::vector<double> d[2],wt[2],c(n,NAN);

        // how far each edge is from where it should be on each line, if not too far, and how much
        // that counts for: an edge that is not much brighter than the blanking is harder to place
        for (unsigned int k=0;k < 2u;k++) {
            d[k].assign(n,NAN);
            wt[k].assign(n,0.0);
            if (!use[k])
                continue;

            for (unsigned int j=0;j < n;j++) {
                const unsigned int y = p + (j * fields);
                const double v = edges[k].pos[y] - ref[k];
                if (fabs(v) <= H) {
                    d[k][j] = v;
                    wt[k][j] = edges[k].step[y] * edges[k].step[y];
                }
            }
            reject_jumps(d[k],jump);

            // too few lines to go by
            if (count_known(d[k]) < std::max(4u,n / 16u))
                d[k].assign(n,NAN);
        }

        // How much longer the lines are, from the lines where the edges agree, counting each line as
        // much as its weaker edge. That is taken to be the same all down the field: it changes slowly,
        // and by too little to tell on each line from edges that are harder to place on some lines.
        double s = 0;
        if (both) {
            std::vector<double> sl(n,NAN),sw(n,0.0);

            settle_disagreements(d,wt,apart);
            for (unsigned int j=0;j < n;j++) {
                if (!isnan(d[0][j]) && !isnan(d[1][j])) {
                    sl[j] = d[1][j] - d[0][j];
                    sw[j] = std::min(wt[0][j],wt[1][j]);
                }
            }

            s = weighted_median(sl,sw);
            if (isnan(s))
                s = 0;
        }

        // and how far the middle of each line moved, from each edge and how much longer the line is
        for (unsigned int j=0;j < n;j++) {
            double tw = 0,tv = 0;

            if (!isnan(d[0][j])) {
                tw += wt[0][j];
                tv += wt[0][j] * (d[0][j] + (s / 2.0));
            }
            if (!isnan(d[1][j])) {
                tw += wt[1][j];
                tv += wt[1][j] * (d[1][j] - (s / 2.0));
            }
            if (tw > 0.0)
                c[j] = tv / tw;
        }

        // nothing to go by in this field: leave it where it is
        if (fill_gaps(c) == 0u)
            continue;

        for (unsigned int j=0;j < n;j++) {
            const double dl = std::max(-H,std::min(H,c[j] - (s / 2.0)));
            const double dr = std::max(-H,std::min(H,c[j] + (s / 2.0)));
            hwarp_t &w = warp[p + (j * fields)];

            if (both) {
                w.stretch = (dr - dl) / width;
                w.shift = dl - (w.stretch * ref[0]);
            }
            else {
                w.shift = use[0] ? dl : dr;
            }
        }
    }
}
