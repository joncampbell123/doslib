#include <math.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "cram.h"

/////////////////////////////////////////////////////////////////////////////
// the packets

bool cram_map_t::parse(const uint8_t *buf,size_t size,bool eight_bit) {
    // as FFmpeg's decoder: a packet too small to be a frame is left out
    if (size < (size_t)((bw * bh) / 512))
        return false;

    std::fill(coded.begin(),coded.end(),0);

    // the blocks go left to right, from the bottom row of blocks up. A skip code skips the block it is in
    // and the count of blocks after it less one. The frame ends where the packet does.
    size_t sp = 0;
    int skip = 0;
    int total = bw * bh;
    for (int by=bh - 1;by >= 0;by--) {
        for (int bx=0;bx < bw;bx++) {
            if (skip > 0) {
                skip--;
                total--;
                continue;
            }
            if (sp + 2u > size)
                return true;

            const unsigned int a = buf[sp++],b = buf[sp++];
            cram_block_t &k = at(bx,by);
            uint8_t mode = CRAM_SOLID;

            if (a == 0 && b == 0 && total == 0)
                return true;
            if ((b & 0xFCu) == 0x84u) {
                skip = (int)(((b - 0x84u) << 8u) + a) - 1;
                total--;
                continue;
            }
            if (eight_bit) {
                if (b < 0x80u) {
                    if (sp + 2u > size) return true;
                    mode = CRAM_TWO;
                    sp += 2u;
                }
                else if (b >= 0x90u) {
                    if (sp + 8u > size) return true;
                    mode = CRAM_EIGHT;
                    sp += 8u;
                }
            }
            else if (b < 0x80u) {
                if (sp + 4u > size) return true;
                // the top bit of the first color says there are eight
                if (buf[sp + 1u] & 0x80u) {
                    if (sp + 16u > size) return true;
                    mode = CRAM_EIGHT;
                    sp += 16u;
                }
                else {
                    mode = CRAM_TWO;
                    sp += 4u;
                }
            }

            k.mode = mode;
            k.flags = (uint16_t)((b << 8u) | a);
            coded[((size_t)by * bw) + bx] = 1;
            total--;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// putting the frame back

namespace {

// A set of pixels coded as one color. A set of a two color block or quarter has the other set of it as its
// pair, and what each pixel can have been is measured along the way from the color of the 0 bits (c0) to
// the color of the 1 bits (c0 + d), as alpha: the encoder gave each pixel the color it was nearest to, or
// split them at the mean of the block, which is alpha = (pixels of 1 bits) / (pixels), so that each pixel
// of 0 bits was below the higher of the two, and each of 1 bits above the lower. Each color is the mean of
// the pixels it was given to.
enum { SET_SOLID=0,SET_PAIR };

struct set_t {
    int                         kind = SET_SOLID;
    int                         n = 0;
    int                         pix[16];            // offsets of the pixels in the frame
    float                       c[3];               // the color the set was coded as
    float                       c0[3],d[3];         // pairs: the way from one color to the other
    float                       inv_dd = 0;
    float                       lo = 0,hi = 0;      // pairs: how far along the way each pixel can be
    float                       perp = 0;           // pairs: how far off the way each pixel can be
    float                       range = 0;          // one color: how far from it each pixel can be
};

struct frame_t {
    int                         w,h;
    std::vector<float>          p[3];

    void alloc(int width,int height) {
        w = width;
        h = height;
        for (int i=0;i < 3;i++)
            p[i].assign((size_t)w * h,0.0f);
    }
    // plane i at (x,y), between pixels
    float at(int i,float x,float y) const {
        if (x < 0.0f) x = 0.0f;
        if (y < 0.0f) y = 0.0f;
        if (x > (float)(w - 1)) x = (float)(w - 1);
        if (y > (float)(h - 1)) y = (float)(h - 1);
        const int x0 = (int)x,y0 = (int)y;
        const int x1 = x0 + 1 < w ? x0 + 1 : x0,y1 = y0 + 1 < h ? y0 + 1 : y0;
        const float fx = x - (float)x0,fy = y - (float)y0;
        const float *v = &p[i][0];
        const float a = v[((size_t)y0 * w) + x0] + ((v[((size_t)y0 * w) + x1] - v[((size_t)y0 * w) + x0]) * fx);
        const float b = v[((size_t)y1 * w) + x0] + ((v[((size_t)y1 * w) + x1] - v[((size_t)y1 * w) + x0]) * fx);
        return a + ((b - a) * fy);
    }
};

// The sets of a frame, from the blocks: each block of one color a set, each two color block or quarter two.
// And how much each pixel is smoothed: those of no block, or left as they are, not at all.
static void make_sets(std::vector<set_t> &sets,std::vector<float> &rate,const float *in,int w,int h,const cram_map_t &map,const cram_params_t &p) {
    const size_t plane = (size_t)w * h;

    sets.clear();
    rate.assign(plane,0.0f);

    for (int by=0;by < map.bh;by++) {
        for (int bx=0;bx < map.bw;bx++) {
            const cram_block_t &blk = map.at(bx,by);
            if (blk.mode == CRAM_UNKNOWN || (blk.mode == CRAM_SOLID && !p.solid))
                continue;

            // the pixels of each quarter and bit
            const float r = blk.mode == CRAM_SOLID ? 1.0f : p.rate;
            set_t s[4][2];
            for (int py=0;py < 4;py++) {
                for (int px=0;px < 4;px++) {
                    int q,bit;
                    cram_pixel_set(blk,px,py,q,bit);
                    if (blk.mode != CRAM_EIGHT)
                        q = 0;
                    const int o = (((by * 4) + py) * w) + (bx * 4) + px;
                    set_t &t = s[q][bit];
                    t.pix[t.n++] = o;
                    rate[(size_t)o] = r;
                }
            }

            for (int q=0;q < 4;q++) {
                for (int bit=0;bit < 2;bit++) {
                    set_t &t = s[q][bit];
                    if (t.n == 0)
                        continue;
                    for (int i=0;i < 3;i++)
                        t.c[i] = in[((size_t)i * plane) + (size_t)t.pix[0]];
                    t.range = p.flat;
                }

                set_t &a = s[q][0],&b = s[q][1];
                if (a.n > 0 && b.n > 0) {
                    float dd = 0;
                    for (int i=0;i < 3;i++) {
                        a.c0[i] = b.c0[i] = a.c[i];
                        a.d[i] = b.d[i] = b.c[i] - a.c[i];
                        dd += a.d[i] * a.d[i];
                    }
                    // Two colors no more different than the noise are noise: the encoder made the pixels of a flat
                    // part one or the other, which shows as speckles. They are taken as one color, their mean, which
                    // each pixel can be as far from as the two colors are apart, and smoothed as a block of one color.
                    if (dd < p.noise * p.noise) {
                        for (int k=0;k < b.n;k++)
                            a.pix[a.n + k] = b.pix[k];
                        for (int i=0;i < 3;i++)
                            a.c[i] = ((a.c[i] * (float)a.n) + (b.c[i] * (float)b.n)) / (float)(a.n + b.n);
                        a.n += b.n;
                        b.n = 0;
                        a.range = std::max(p.flat,sqrtf(dd));
                        for (int k=0;k < a.n;k++)
                            rate[(size_t)a.pix[k]] = 1.0f;
                    }
                    // two colors no more different than the rounding of them are as good as one
                    else if (dd >= 4.0f) {
                        const float split = (float)b.n / (float)(a.n + b.n);
                        a.kind = b.kind = SET_PAIR;
                        a.inv_dd = b.inv_dd = 1.0f / dd;
                        a.lo = -p.spread;
                        a.hi = std::max(0.5f,split);
                        b.lo = std::min(0.5f,split);
                        b.hi = 1.0f + p.spread;
                        a.perp = b.perp = std::max(p.flat * 0.5f,sqrtf(dd) * 0.25f);
                    }
                }
                for (int bit=0;bit < 2;bit++) {
                    if (s[q][bit].n > 0)
                        sets.push_back(s[q][bit]);
                }
            }
        }
    }
}

// each pixel back within what the code of its set says it can have been: within reach of its color, and the
// mean of the set its color, each of which can undo the other a little, so done again for each round
static void project(frame_t &f,const std::vector<set_t> &sets,const cram_params_t &p,int rounds) {
    const int n = (int)sets.size();

#pragma omp parallel for schedule(static)
    for (int si=0;si < n;si++) {
        const set_t &s = sets[(size_t)si];
        float *pl[3] = { &f.p[0][0],&f.p[1][0],&f.p[2][0] };

        // each pixel within reach of the color
        auto clamp = [&](void) {
            for (int k=0;k < s.n;k++) {
                const int o = s.pix[k];
                if (s.kind == SET_SOLID) {
                    for (int i=0;i < 3;i++)
                        pl[i][o] = std::min(s.c[i] + s.range,std::max(s.c[i] - s.range,pl[i][o]));
                    continue;
                }

                float v[3],a = 0;
                for (int i=0;i < 3;i++) {
                    v[i] = pl[i][o] - s.c0[i];
                    a += v[i] * s.d[i];
                }
                a *= s.inv_dd;
                const float ac = std::min(s.hi,std::max(s.lo,a));
                // and not too far off the way from one color to the other
                float q[3],qq = 0;
                for (int i=0;i < 3;i++) {
                    q[i] = v[i] - (a * s.d[i]);
                    qq += q[i] * q[i];
                }
                const float k2 = qq > s.perp * s.perp ? s.perp / sqrtf(qq) : 1.0f;
                for (int i=0;i < 3;i++)
                    pl[i][o] = s.c0[i] + (ac * s.d[i]) + (q[i] * k2);
            }
        };

        // the mean of the set its color, within the rounding of it
        auto mean = [&](void) {
            for (int i=0;i < 3;i++) {
                float m = 0;
                for (int k=0;k < s.n;k++)
                    m += pl[i][s.pix[k]];
                m /= (float)s.n;
                float e = 0;
                if (m > s.c[i] + p.tolerance) e = (s.c[i] + p.tolerance) - m;
                else if (m < s.c[i] - p.tolerance) e = (s.c[i] - p.tolerance) - m;
                if (e != 0.0f) {
                    for (int k=0;k < s.n;k++)
                        pl[i][s.pix[k]] += e;
                }
            }
        };

        clamp();
        for (int r=0;r < rounds;r++) {
            mean();
            clamp();
        }
    }
}

// a plane blurred by a gaussian of sigma, in place
static void blur(std::vector<float> &v,std::vector<float> &tmp,int w,int h,float sigma) {
    const int r = std::max(1,(int)ceilf(sigma * 2.5f));
    std::vector<float> k((size_t)r + 1u);
    float sum = 0;
    for (int j=0;j <= r;j++) {
        k[(size_t)j] = expf(-(float)(j * j) / (2.0f * sigma * sigma));
        sum += j ? 2.0f * k[(size_t)j] : k[(size_t)j];
    }
    for (int j=0;j <= r;j++)
        k[(size_t)j] /= sum;
    tmp.resize(v.size());

#pragma omp parallel for schedule(static)
    for (int y=0;y < h;y++) {
        const float *s = &v[(size_t)y * w];
        float *d = &tmp[(size_t)y * w];
        for (int x=0;x < w;x++) {
            float a = s[x] * k[0];
            for (int j=1;j <= r;j++)
                a += (s[std::max(0,x - j)] + s[std::min(w - 1,x + j)]) * k[(size_t)j];
            d[x] = a;
        }
    }
#pragma omp parallel for schedule(static)
    for (int y=0;y < h;y++) {
        float *d = &v[(size_t)y * w];
        for (int x=0;x < w;x++) {
            float a = tmp[((size_t)y * w) + x] * k[0];
            for (int j=1;j <= r;j++)
                a += (tmp[((size_t)std::max(0,y - j) * w) + x] + tmp[((size_t)std::min(h - 1,y + j) * w) + x]) * k[(size_t)j];
            d[x] = a;
        }
    }
}

} // namespace

void cram_restore(const float *in,float *out,int w,int h,const cram_map_t &map,const cram_params_t &p) {
    const size_t plane = (size_t)w * h;
    std::vector<set_t> sets;
    std::vector<float> rate;
    frame_t cur,next;

    make_sets(sets,rate,in,w,h,map,p);
    cur.alloc(w,h);
    next.alloc(w,h);
    for (int i=0;i < 3;i++)
        memcpy(&cur.p[i][0],in + ((size_t)i * plane),plane * sizeof(float));

    // The blocks of one color, to start with, the colors of the blocks around them blended across them, as
    // a picture a quarter the size scaled up, except from blocks of colors too different, which are of
    // something else. Smoothing would take as many turns as the block is pixels across to get there.
    if (p.solid && map.bw > 0 && map.bh > 0) {
        std::vector<float> mean((size_t)map.bw * map.bh * 3,0.0f);
        for (int by=0;by < map.bh;by++) {
            for (int bx=0;bx < map.bw;bx++) {
                for (int i=0;i < 3;i++) {
                    float m = 0;
                    for (int py=0;py < 4;py++)
                        for (int px=0;px < 4;px++)
                            m += in[((size_t)i * plane) + ((size_t)((by * 4) + py) * w) + (bx * 4) + px];
                    mean[((((size_t)by * map.bw) + bx) * 3) + i] = m / 16.0f;
                }
            }
        }

        const float sigma2 = 2.0f * (p.flat * 1.5f) * (p.flat * 1.5f);
#pragma omp parallel for schedule(static)
        for (int by=0;by < map.bh;by++) {
            for (int bx=0;bx < map.bw;bx++) {
                if (map.at(bx,by).mode != CRAM_SOLID)
                    continue;
                const float *self = &mean[(((size_t)by * map.bw) + bx) * 3];
                for (int py=0;py < 4;py++) {
                    for (int px=0;px < 4;px++) {
                        // where the pixel is between the middles of the blocks
                        const float fx = ((float)px - 1.5f) / 4.0f,fy = ((float)py - 1.5f) / 4.0f;
                        const int ox = fx < 0.0f ? -1 : 0,oy = fy < 0.0f ? -1 : 0;
                        const float tx = fx - (float)ox,ty = fy - (float)oy;
                        float acc[3] = { 0,0,0 },wsum = 0;
                        for (int j=0;j < 2;j++) {
                            for (int k=0;k < 2;k++) {
                                const int nx = std::min(map.bw - 1,std::max(0,bx + ox + k));
                                const int ny = std::min(map.bh - 1,std::max(0,by + oy + j));
                                const float *m = &mean[(((size_t)ny * map.bw) + nx) * 3];
                                float dd = 0;
                                for (int i=0;i < 3;i++)
                                    dd += (m[i] - self[i]) * (m[i] - self[i]);
                                const float wt = (k ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty) * expf(-dd / sigma2);
                                for (int i=0;i < 3;i++)
                                    acc[i] += m[i] * wt;
                                wsum += wt;
                            }
                        }
                        const size_t o = ((size_t)((by * 4) + py) * w) + (bx * 4) + px;
                        for (int i=0;i < 3;i++)
                            cur.p[i][o] = wsum > 0.0f ? acc[i] / wsum : self[i];
                    }
                }
            }
        }
        project(cur,sets,p,1);
    }

    // Smoothing, each turn a step of diffusion: along the edges, of what the luma of the picture says the
    // edges are, and across them only where there is little change, so that the steps of the edges are
    // taken out without blurring them, and the picture where it changes little made smooth. The blocks of one
    // color are smoothed the most, and the blocks of two or eight colors a fraction of that (the rate), as their
    // bits have the detail, much of which smoothing them as much would take out.
    std::vector<float> jxx(plane),jxy(plane),jyy(plane),tmp;
    std::vector<float> tx(plane),ty(plane),mt(plane),mn(plane);
    const float e2 = p.edge * p.edge;
    const float dt = 0.24f;

    for (int it=0;it < p.iterations;it++) {
        // the structure tensor, every other turn: the luma's gradients, multiplied, and blurred
        if ((it & 1) == 0) {
#pragma omp parallel for schedule(static)
            for (int y=0;y < h;y++) {
                const int y0 = std::max(0,y - 1),y1 = std::min(h - 1,y + 1);
                for (int x=0;x < w;x++) {
                    const int x0 = std::max(0,x - 1),x1 = std::min(w - 1,x + 1);
                    auto L = [&](int xx,int yy) -> float {
                        const size_t o = ((size_t)yy * w) + xx;
                        return (cur.p[0][o] * 0.299f) + (cur.p[1][o] * 0.587f) + (cur.p[2][o] * 0.114f);
                    };
                    // Scharr, which is nearer the same whichever way the edge goes
                    const float gx = ((3.0f * (L(x1,y0) - L(x0,y0))) + (10.0f * (L(x1,y) - L(x0,y))) + (3.0f * (L(x1,y1) - L(x0,y1)))) / 32.0f;
                    const float gy = ((3.0f * (L(x0,y1) - L(x0,y0))) + (10.0f * (L(x,y1) - L(x,y0))) + (3.0f * (L(x1,y1) - L(x1,y0)))) / 32.0f;
                    const size_t o = ((size_t)y * w) + x;
                    jxx[o] = gx * gx;
                    jxy[o] = gx * gy;
                    jyy[o] = gy * gy;
                }
            }
            blur(jxx,tmp,w,h,p.scale);
            blur(jxy,tmp,w,h,p.scale);
            blur(jyy,tmp,w,h,p.scale);

#pragma omp parallel for schedule(static)
            for (int y=0;y < h;y++) {
                for (int x=0;x < w;x++) {
                    const size_t o = ((size_t)y * w) + x;
                    const float a = jxx[o],b = jxy[o],c = jyy[o];
                    const float r = sqrtf(((a - c) * (a - c)) + (4.0f * b * b));
                    const float l1 = (a + c + r) * 0.5f,l2 = (a + c - r) * 0.5f;
                    // the way across the edge, and along it
                    float vx = 2.0f * b,vy = c - a + r;
                    float vl = sqrtf((vx * vx) + (vy * vy));
                    if (vl < 1e-6f) {
                        vx = a >= c ? 1.0f : 0.0f;
                        vy = a >= c ? 0.0f : 1.0f;
                    }
                    else {
                        vx /= vl;
                        vy /= vl;
                    }
                    // little change: smoothed every way. An edge: along it, the more the more it goes one way,
                    // not at all across it, and a corner, which goes both ways, kept.
                    const float g = 1.0f / (1.0f + (l1 / e2));
                    const float coh = (l1 - l2) / (l1 + l2 + 1e-6f);
                    tx[o] = -vy;
                    ty[o] = vx;
                    mn[o] = g;
                    mt[o] = g + ((1.0f - g) * coh * coh);
                }
            }
        }

#pragma omp parallel for schedule(static)
        for (int y=0;y < h;y++) {
            for (int x=0;x < w;x++) {
                const size_t o = ((size_t)y * w) + x;
                if (rate[o] <= 0.0f) {
                    for (int i=0;i < 3;i++)
                        next.p[i][o] = cur.p[i][o];
                    continue;
                }
                const float k = dt * rate[o];
                const float ax = tx[o],ay = ty[o];
                for (int i=0;i < 3;i++) {
                    const float v = cur.p[i][o];
                    const float along = cur.at(i,(float)x + ax,(float)y + ay) + cur.at(i,(float)x - ax,(float)y - ay) - (2.0f * v);
                    const float across = cur.at(i,(float)x + ay,(float)y - ax) + cur.at(i,(float)x - ay,(float)y + ax) - (2.0f * v);
                    next.p[i][o] = v + (k * ((mt[o] * along) + (mn[o] * across)));
                }
            }
        }
        std::swap(cur,next);
        // the last turn, the pixels put back nearer the both of what their code says
        project(cur,sets,p,it == p.iterations - 1 ? 8 : 1);
    }

    for (int i=0;i < 3;i++)
        memcpy(out + ((size_t)i * plane),&cur.p[i][0],plane * sizeof(float));
}
