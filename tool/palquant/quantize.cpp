
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include <algorithm>

#include "quantize.h"

// How much a color's pixel count weighs when making the palette is the count to this power.
// With the count itself, the palette has the least total error, but a small area of a distinct
// color (the green eyes of a cat in a gray and brown picture, say) gets no palette entry because
// it adds little to the total. A power below 1 gives such colors more say, for a little more
// total error: 0.75 adds about 2-4%, while 0.5 adds 15-25% and shows.
#define COUNT_POWER                 0.75

// k-means stops after this many rounds, or when a round lowers the error by less than this fraction
#define KMEANS_MAX_ROUNDS           16
#define KMEANS_MIN_GAIN             1e-4

static const double axis_weight[3] = { PQ_WEIGHT_R, PQ_WEIGHT_G, PQ_WEIGHT_B };

static inline uint32_t pack_rgb(const rgb_t &c) {
    return ((uint32_t)c.r << 16u) | ((uint32_t)c.g << 8u) | (uint32_t)c.b;
}

static inline uint8_t round_channel(const double v) {
    if (v <= 0.0)
        return 0;
    if (v >= 255.0)
        return 255;

    return (uint8_t)(v + 0.5);
}

color_histogram_t::color_histogram_t() : seen((1u << 24u) / 64u,0), cell(1u << 18u) {
}

void color_histogram_t::add(const rgb_image_t &img) {
    for (size_t i=0;i < img.pixels.size();i++) {
        const rgb_t &c = img.pixels[i];
        const uint32_t p = pack_rgb(c);
        uint64_t &w = seen[p >> 6u];
        const uint64_t bit = (uint64_t)1u << (p & 63u);

        if (!(w & bit)) {
            w |= bit;
            if (distinct < MAX_EXACT)
                exact.push_back(c);
            if (distinct <= MAX_EXACT)
                distinct++;
        }

        cell_t &ce = cell[((uint32_t)(c.r >> 2u) << 12u) | ((uint32_t)(c.g >> 2u) << 6u) | (uint32_t)(c.b >> 2u)];
        ce.count++;
        ce.r += c.r;
        ce.g += c.g;
        ce.b += c.b;
    }
}

// the colors in one cell of the histogram, by their average
struct sample_t {
    double                      c[3];
    double                      w;                  // how much it weighs, from how many pixels (see COUNT_POWER)
};

// a box of samples for median cut, samples[begin..end)
struct box_t {
    size_t                      begin,end;
    double                      err;                // weighted sum of the squared distances from the box's average
};

static void box_mean(const std::vector<sample_t> &s,size_t begin,size_t end,double mean[3],double &w) {
    double sum[3] = { 0,0,0 };

    w = 0;
    for (size_t i=begin;i < end;i++) {
        w += s[i].w;
        for (unsigned int k=0;k < 3;k++)
            sum[k] += s[i].w * s[i].c[k];
    }

    for (unsigned int k=0;k < 3;k++)
        mean[k] = (w > 0) ? (sum[k] / w) : 0;
}

static double box_error(const std::vector<sample_t> &s,size_t begin,size_t end) {
    double mean[3],w,err = 0;

    box_mean(s,begin,end,mean,w);
    for (size_t i=begin;i < end;i++) {
        for (unsigned int k=0;k < 3;k++) {
            const double d = s[i].c[k] - mean[k];
            err += s[i].w * axis_weight[k] * d * d;
        }
    }

    return err;
}

// Split a box in two along the axis where its colors spread the most, at the point that leaves
// the least error in the two halves. Returns where the second half begins.
static size_t split_box(std::vector<sample_t> &s,const box_t &b) {
    double mean[3],w,spread[3] = { 0,0,0 };
    unsigned int axis = 0;

    box_mean(s,b.begin,b.end,mean,w);
    for (size_t i=b.begin;i < b.end;i++) {
        for (unsigned int k=0;k < 3;k++) {
            const double d = s[i].c[k] - mean[k];
            spread[k] += s[i].w * axis_weight[k] * d * d;
        }
    }
    for (unsigned int k=1;k < 3;k++) {
        if (spread[k] > spread[axis])
            axis = k;
    }

    std::sort(s.begin() + (ptrdiff_t)b.begin,s.begin() + (ptrdiff_t)b.end,[axis](const sample_t &x,const sample_t &y) {
        return x.c[axis] < y.c[axis];
    });

    // Sums of the samples' weights, and of their distances and squared distances from the box's
    // average, to the left of each split point. The error of either half follows from those.
    double tw = 0,td[3] = { 0,0,0 },tq[3] = { 0,0,0 };
    for (size_t i=b.begin;i < b.end;i++) {
        tw += s[i].w;
        for (unsigned int k=0;k < 3;k++) {
            const double d = s[i].c[k] - mean[k];
            td[k] += s[i].w * d;
            tq[k] += s[i].w * d * d;
        }
    }

    double lw = 0,ld[3] = { 0,0,0 },lq[3] = { 0,0,0 };
    double best_err = -1;
    size_t best = b.begin + 1;
    for (size_t i=b.begin;(i + 1) < b.end;i++) {
        lw += s[i].w;
        for (unsigned int k=0;k < 3;k++) {
            const double d = s[i].c[k] - mean[k];
            ld[k] += s[i].w * d;
            lq[k] += s[i].w * d * d;
        }

        // only split between different values, so that both halves are worth splitting further
        if (s[i].c[axis] == s[i+1].c[axis])
            continue;

        const double rw = tw - lw;
        double err = 0;
        for (unsigned int k=0;k < 3;k++) {
            const double rd = td[k] - ld[k],rq = tq[k] - lq[k];
            err += axis_weight[k] * ((lq[k] - (ld[k] * ld[k] / lw)) + (rq - (rd * rd / rw)));
        }

        if (best_err < 0 || err < best_err) {
            best_err = err;
            best = i + 1;
        }
    }

    return best;
}

static rgb_t box_color(const std::vector<sample_t> &s,const box_t &b) {
    double mean[3],w;
    rgb_t c;

    box_mean(s,b.begin,b.end,mean,w);
    c.r = round_channel(mean[0]);
    c.g = round_channel(mean[1]);
    c.b = round_channel(mean[2]);
    return c;
}

static std::vector<rgb_t> median_cut(std::vector<sample_t> &s,unsigned int colors) {
    std::vector<box_t> boxes;
    box_t all;

    all.begin = 0;
    all.end = s.size();
    all.err = box_error(s,all.begin,all.end);
    boxes.push_back(all);

    // split the box with the most error until there are enough colors, or no box is worth splitting
    while (boxes.size() < colors) {
        size_t pick = boxes.size();
        for (size_t i=0;i < boxes.size();i++) {
            if ((boxes[i].end - boxes[i].begin) >= 2u && boxes[i].err > 0 && (pick == boxes.size() || boxes[i].err > boxes[pick].err))
                pick = i;
        }
        if (pick == boxes.size())
            break;

        const box_t b = boxes[pick];
        const size_t mid = split_box(s,b);
        box_t lo,hi;

        lo.begin = b.begin;
        lo.end = mid;
        lo.err = box_error(s,lo.begin,lo.end);
        hi.begin = mid;
        hi.end = b.end;
        hi.err = box_error(s,hi.begin,hi.end);

        boxes[pick] = lo;
        boxes.push_back(hi);
    }

    std::vector<rgb_t> pal;
    for (size_t i=0;i < boxes.size();i++)
        pal.push_back(box_color(s,boxes[i]));

    return pal;
}

// Improve the palette with rounds of k-means: give each sample to its nearest palette entry,
// then move each entry to the average of its samples.
static void refine_palette(const std::vector<sample_t> &s,std::vector<rgb_t> &pal) {
    std::vector<rgb_t> sc(s.size());
    for (size_t i=0;i < s.size();i++) {
        sc[i].r = round_channel(s[i].c[0]);
        sc[i].g = round_channel(s[i].c[1]);
        sc[i].b = round_channel(s[i].c[2]);
    }

    double last_err = -1;
    for (unsigned int round=0;round < KMEANS_MAX_ROUNDS;round++) {
        const nearest_color_t nc(pal);
        std::vector<double> sum(pal.size() * 3u,0.0),w(pal.size(),0.0);
        std::vector<double> sample_err(s.size(),0.0);
        double err = 0;

        for (size_t i=0;i < s.size();i++) {
            const unsigned int p = nc.find(sc[i]);
            sample_err[i] = s[i].w * color_distance(sc[i],pal[p]);
            err += sample_err[i];
            w[p] += s[i].w;
            for (unsigned int k=0;k < 3;k++)
                sum[(p * 3u) + k] += s[i].w * s[i].c[k];
        }

        if (last_err >= 0 && (last_err - err) <= (last_err * KMEANS_MIN_GAIN))
            break;
        last_err = err;

        for (size_t p=0;p < pal.size();p++) {
            if (w[p] > 0) {
                pal[p].r = round_channel(sum[(p * 3u) + 0u] / w[p]);
                pal[p].g = round_channel(sum[(p * 3u) + 1u] / w[p]);
                pal[p].b = round_channel(sum[(p * 3u) + 2u] / w[p]);
            }
            else {
                // nothing is nearest to this entry. Move it to the sample with the most error, which is better than wasting it.
                size_t worst = 0;
                for (size_t i=1;i < s.size();i++) {
                    if (sample_err[i] > sample_err[worst])
                        worst = i;
                }

                pal[p] = sc[worst];
                sample_err[worst] = 0;
            }
        }
    }
}

std::vector<rgb_t> make_palette(const color_histogram_t &hist,unsigned int colors) {
    std::vector<rgb_t> pal;

    if (hist.distinct_colors() <= colors) {
        pal = hist.exact_colors();
    }
    else {
        const std::vector<color_histogram_t::cell_t> &cells = hist.cells();
        std::vector<sample_t> s;

        for (size_t i=0;i < cells.size();i++) {
            if (cells[i].count != 0) {
                const double n = (double)cells[i].count;
                sample_t sm;
                sm.c[0] = (double)cells[i].r / n;
                sm.c[1] = (double)cells[i].g / n;
                sm.c[2] = (double)cells[i].b / n;
                sm.w = pow(n,COUNT_POWER);
                s.push_back(sm);
            }
        }

        pal = median_cut(s,colors);
        refine_palette(s,pal);

        // entries that ended up the same color are only needed once
        std::sort(pal.begin(),pal.end(),[](const rgb_t &a,const rgb_t &b) { return pack_rgb(a) < pack_rgb(b); });
        pal.erase(std::unique(pal.begin(),pal.end(),[](const rgb_t &a,const rgb_t &b) { return pack_rgb(a) == pack_rgb(b); }),pal.end());
    }

    // darkest first
    std::sort(pal.begin(),pal.end(),[](const rgb_t &a,const rgb_t &b) {
        const uint32_t la = (299u * a.r) + (587u * a.g) + (114u * a.b);
        const uint32_t lb = (299u * b.r) + (587u * b.g) + (114u * b.b);
        if (la != lb)
            return la < lb;

        return pack_rgb(a) < pack_rgb(b);
    });

    return pal;
}

nearest_color_t::nearest_color_t(const std::vector<rgb_t> &palette) {
    for (size_t i=0;i < palette.size();i++) {
        entry_t e;
        e.c = palette[i];
        e.index = (unsigned int)i;
        by_green.push_back(e);
    }

    std::sort(by_green.begin(),by_green.end(),[](const entry_t &a,const entry_t &b) {
        if (a.c.g != b.c.g)
            return a.c.g < b.c.g;

        return a.index < b.index;
    });
}

unsigned int nearest_color_t::find(const rgb_t &c) const {
    // Start at the entries with the nearest green, and work outward both ways. Once the green
    // difference alone is as far as the best so far, nothing further out that way can be nearer.
    const size_t n = by_green.size();
    size_t hi = (size_t)(std::lower_bound(by_green.begin(),by_green.end(),c.g,[](const entry_t &e,const uint8_t g) { return e.c.g < g; }) - by_green.begin());
    size_t lo = hi;
    uint32_t best = UINT32_MAX;
    unsigned int best_index = 0;
    bool up = hi < n,down = lo > 0;

    while (up || down) {
        if (up) {
            const entry_t &e = by_green[hi];
            const int dg = (int)e.c.g - (int)c.g;

            if ((PQ_WEIGHT_G * (uint32_t)(dg * dg)) >= best) {
                up = false;
            }
            else {
                const uint32_t d = color_distance(e.c,c);
                if (d < best) {
                    best = d;
                    best_index = e.index;
                }
                if (++hi >= n)
                    up = false;
            }
        }

        if (down) {
            const entry_t &e = by_green[lo - 1u];
            const int dg = (int)c.g - (int)e.c.g;

            if ((PQ_WEIGHT_G * (uint32_t)(dg * dg)) >= best) {
                down = false;
            }
            else {
                const uint32_t d = color_distance(e.c,c);
                if (d < best) {
                    best = d;
                    best_index = e.index;
                }
                if (--lo == 0)
                    down = false;
            }
        }
    }

    return best_index;
}

