#include <math.h>
#include <stdlib.h>

#include <algorithm>
#include <vector>

#include "motion.h"

void build_pyramid(pyramid_t &pyr,const image_t &full,unsigned int levels) {
    pyr.level.resize(1);

    image8_t &l0 = pyr.level[0];
    l0.alloc(full.w,full.h);
    for (size_t i=0;i < full.v.size();i++)
        l0.v[i] = (uint8_t)std::min(255.0f,std::max(0.0f,full.v[i] + 0.5f));

    while (pyr.level.size() < levels && pyr.level.back().w >= 32 && pyr.level.back().h >= 32) {
        const image8_t &a = pyr.level.back();
        image8_t b;

        b.alloc(a.w / 2,a.h / 2);
        for (int y=0;y < b.h;y++) {
            const uint8_t *r0 = a.row(y * 2),*r1 = a.row((y * 2) + 1);
            uint8_t *d = b.row(y);
            for (int x=0;x < b.w;x++)
                d[x] = (uint8_t)(((unsigned int)r0[x * 2] + r0[(x * 2) + 1] + r1[x * 2] + r1[(x * 2) + 1] + 2u) >> 2u);
        }

        pyr.level.push_back(b);
    }

    // half a pixel over, half a line down, and both, the edges going on outside the image
    const image8_t &a = pyr.level[0];
    for (int i=0;i < 3;i++)
        pyr.half[i].alloc(a.w,a.h);
    for (int y=0;y < a.h;y++) {
        const uint8_t *r0 = a.row(y),*r1 = a.row(y + 1 < a.h ? y + 1 : y);
        uint8_t *hx = pyr.half[0].row(y),*hy = pyr.half[1].row(y),*hxy = pyr.half[2].row(y);
        for (int x=0;x < a.w;x++) {
            const int x1 = x + 1 < a.w ? x + 1 : x;
            hx[x] = (uint8_t)(((unsigned int)r0[x] + r0[x1] + 1u) >> 1u);
            hy[x] = (uint8_t)(((unsigned int)r0[x] + r1[x] + 1u) >> 1u);
            hxy[x] = (uint8_t)(((unsigned int)r0[x] + r0[x1] + r1[x] + r1[x1] + 2u) >> 2u);
        }
    }
}

// the mean difference, each pixel's at most trunc, of the square of cur 2*hs across centered on (cx,cy)
// and the same square of ref moved by whole pixels (dx,dy). The edges of ref go on outside it.
static float cost_int(const image8_t &cur,const image8_t &ref,int cx,int cy,int hs,int dx,int dy,int trunc) {
    const int x0 = std::max(0,cx - hs),x1 = std::min(cur.w,cx + hs);
    const int y0 = std::max(0,cy - hs),y1 = std::min(cur.h,cy + hs);
    int sum = 0;

    if (x1 <= x0 || y1 <= y0)
        return (float)trunc;

    if (x0 + dx >= 0 && x1 + dx <= ref.w && y0 + dy >= 0 && y1 + dy <= ref.h) {
        const int n = x1 - x0;
        for (int y=y0;y < y1;y++) {
            const uint8_t *c = cur.row(y) + x0;
            const uint8_t *r = ref.row(y + dy) + x0 + dx;
            for (int x=0;x < n;x++)
                sum += std::min(abs((int)c[x] - (int)r[x]),trunc);
        }
    }
    else {
        for (int y=y0;y < y1;y++) {
            const uint8_t *c = cur.row(y);
            const uint8_t *r = ref.row(std::min(std::max(y + dy,0),ref.h - 1));
            for (int x=x0;x < x1;x++)
                sum += std::min(abs((int)c[x] - (int)r[std::min(std::max(x + dx,0),ref.w - 1)]),trunc);
        }
    }

    return (float)sum / (float)((x1 - x0) * (y1 - y0));
}

// the same at full size, moved by whole or half pixels
static float cost_half(const pyramid_t &cur,const pyramid_t &ref,int cx,int cy,int hs,float dx,float dy,int trunc) {
    const int ix = (int)floorf(dx),iy = (int)floorf(dy);
    const bool hx = (dx != (float)ix),hy = (dy != (float)iy);
    const image8_t &r = hx ? (hy ? ref.half[2] : ref.half[0]) : (hy ? ref.half[1] : ref.level[0]);

    return cost_int(cur.level[0],r,cx,cy,hs,ix,iy,trunc);
}

void mvfield_t::at(float x,float y,float &dx,float &dy,float *c) const {
    float fx = (x - (float)(block / 2)) / (float)block;
    float fy = (y - (float)(block / 2)) / (float)block;

    if (fx < 0.0f) fx = 0.0f;
    if (fy < 0.0f) fy = 0.0f;
    if (fx > (float)(bw - 1)) fx = (float)(bw - 1);
    if (fy > (float)(bh - 1)) fy = (float)(bh - 1);

    const int x0 = (int)fx,y0 = (int)fy;
    const int x1 = x0 + 1 < bw ? x0 + 1 : x0;
    const int y1 = y0 + 1 < bh ? y0 + 1 : y0;
    const float ax = fx - (float)x0,ay = fy - (float)y0;
    const size_t i00 = ((size_t)y0 * bw) + x0,i01 = ((size_t)y0 * bw) + x1;
    const size_t i10 = ((size_t)y1 * bw) + x0,i11 = ((size_t)y1 * bw) + x1;
    const float w00 = (1.0f - ax) * (1.0f - ay),w01 = ax * (1.0f - ay);
    const float w10 = (1.0f - ax) * ay,w11 = ax * ay;

    dx = (vx[i00] * w00) + (vx[i01] * w01) + (vx[i10] * w10) + (vx[i11] * w11);
    dy = (vy[i00] * w00) + (vy[i01] * w01) + (vy[i10] * w10) + (vy[i11] * w11);
    if (c != NULL)
        *c = (cost[i00] * w00) + (cost[i01] * w01) + (cost[i10] * w10) + (cost[i11] * w11);
}

void estimate_motion(mvfield_t &mv,const pyramid_t &cur,const pyramid_t &ref,const me_params_t &mp) {
    // half the size of the square matched at each level: at the levels of smaller images it is a bigger
    // part of the picture, so that there is enough of it to match
    static const int half_size[] = { 8, 6, 4, 3 };
    const int levels = (int)std::min(std::min(cur.level.size(),ref.level.size()),sizeof(half_size) / sizeof(half_size[0]));
    const int top = levels - 1;
    const int B = mp.block;
    const image8_t &c0 = cur.level[0];

    mv.alloc(c0.w,c0.h,B);

    const int nb = mv.bw * mv.bh;
    std::vector<int> ix(nb,0),iy(nb,0);

    auto center = [&](int b,int level,int &cx,int &cy) {
        cx = std::min(((b % mv.bw) * B) + (B / 2),c0.w - 1) >> level;
        cy = std::min(((b / mv.bw) * B) + (B / 2),c0.h - 1) >> level;
    };

    // the whole range, at the smallest level
    {
        const int rx = std::max(1,mp.range_x >> top),ry = std::max(1,mp.range_y >> top);
        const float lam = mp.lambda * (float)(1 << top);

        #pragma omp parallel for schedule(dynamic,8)
        for (int b=0;b < nb;b++) {
            int cx,cy,bx = 0,by = 0;
            float best = 1e30f;

            center(b,top,cx,cy);
            for (int dy=-ry;dy <= ry;dy++) {
                for (int dx=-rx;dx <= rx;dx++) {
                    const float c = cost_int(cur.level[top],ref.level[top],cx,cy,half_size[top],dx,dy,mp.trunc) + (lam * (float)(abs(dx) + abs(dy)));
                    if (c < best) {
                        best = c;
                        bx = dx;
                        by = dy;
                    }
                }
            }

            ix[b] = bx;
            iy[b] = by;
        }
    }

    // then a pixel either way at each level after it, to where it is in the bigger image
    for (int level=top - 1;level >= 0;level--) {
        const float lam = mp.lambda * (float)(1 << level);

        #pragma omp parallel for schedule(dynamic,8)
        for (int b=0;b < nb;b++) {
            int cx,cy;
            const int sx = ix[b] * 2,sy = iy[b] * 2;
            int bx = sx,by = sy;
            float best = 1e30f;

            center(b,level,cx,cy);
            for (int dy=sy - 1;dy <= sy + 1;dy++) {
                for (int dx=sx - 1;dx <= sx + 1;dx++) {
                    const float c = cost_int(cur.level[level],ref.level[level],cx,cy,half_size[level],dx,dy,mp.trunc) + (lam * (float)(abs(dx) + abs(dy)));
                    if (c < best) {
                        best = c;
                        bx = dx;
                        by = dy;
                    }
                }
            }

            ix[b] = bx;
            iy[b] = by;
        }
    }

    // A block that matched wrong, such as where it had little in it to match, or a dropout that looks like
    // something somewhere else in the other picture, does not go where the blocks around it went, as the
    // picture does. So each block is matched again, at where it went, where the blocks around it went
    // and the median of them, and a pixel either way of the best, with what it costs to be away from the median.
    for (unsigned int round=0;round < 2u;round++) {
        const std::vector<int> px = ix,py = iy;
        const int hs = half_size[0];
        const bool last = (round == 1u);

        #pragma omp parallel for schedule(dynamic,8)
        for (int b=0;b < nb;b++) {
            const int bxp = b % mv.bw,byp = b / mv.bw;
            int cx,cy;
            int cand[11][2];
            int mx[9],my[9];
            int nc = 0,nm = 0;

            center(b,0,cx,cy);
            for (int ny=std::max(0,byp - 1);ny <= std::min(mv.bh - 1,byp + 1);ny++) {
                for (int nx=std::max(0,bxp - 1);nx <= std::min(mv.bw - 1,bxp + 1);nx++) {
                    const size_t n = ((size_t)ny * mv.bw) + nx;
                    mx[nm] = px[n];
                    my[nm] = py[n];
                    nm++;
                    cand[nc][0] = px[n];
                    cand[nc][1] = py[n];
                    nc++;
                }
            }
            std::nth_element(mx,mx + (nm / 2),mx + nm);
            std::nth_element(my,my + (nm / 2),my + nm);
            const int medx = mx[nm / 2],medy = my[nm / 2];
            cand[nc][0] = medx; cand[nc][1] = medy; nc++;

            auto cost = [&](float dx,float dy) -> float {
                return cost_half(cur,ref,cx,cy,hs,dx,dy,mp.trunc) + (mp.lambda * (fabsf(dx) + fabsf(dy))) +
                    (mp.smooth * (fabsf(dx - (float)medx) + fabsf(dy - (float)medy)));
            };

            int bx = px[b],by = py[b];
            float best = 1e30f;
            for (int i=0;i < nc;i++) {
                bool seen = false;
                for (int j=0;j < i && !seen;j++)
                    seen = (cand[j][0] == cand[i][0] && cand[j][1] == cand[i][1]);
                if (seen)
                    continue;
                const float c = cost((float)cand[i][0],(float)cand[i][1]);
                if (c < best) {
                    best = c;
                    bx = cand[i][0];
                    by = cand[i][1];
                }
            }

            const int sx = bx,sy = by;
            for (int dy=sy - 1;dy <= sy + 1;dy++) {
                for (int dx=sx - 1;dx <= sx + 1;dx++) {
                    if (dx == sx && dy == sy)
                        continue;
                    const float c = cost((float)dx,(float)dy);
                    if (c < best) {
                        best = c;
                        bx = dx;
                        by = dy;
                    }
                }
            }

            ix[b] = bx;
            iy[b] = by;
            if (!last)
                continue;

            // to half a pixel
            float fx = (float)bx,fy = (float)by;
            if (mp.halfpel) {
                const float c0x = fx,c0y = fy;
                for (int dy=-1;dy <= 1;dy++) {
                    for (int dx=-1;dx <= 1;dx++) {
                        if (dx == 0 && dy == 0)
                            continue;
                        const float tx = c0x + (0.5f * (float)dx),ty = c0y + (0.5f * (float)dy);
                        const float c = cost(tx,ty);
                        if (c < best) {
                            best = c;
                            fx = tx;
                            fy = ty;
                        }
                    }
                }
            }

            mv.vx[b] = fx;
            mv.vy[b] = fy;
            mv.cost[b] = cost_half(cur,ref,cx,cy,hs,fx,fy,mp.trunc);
        }
    }
}

void refine_masked(mvfield_t &mv,const image_t &cur,const image_t &ref,const mask_t &cmask,const mask_t &rmask,const me_params_t &mp) {
    const int B = mv.block;
    const int nb = mv.bw * mv.bh;
    const std::vector<float> px = mv.vx,py = mv.vy;
    std::vector<uint8_t> known(nb,1),near(nb,0);
    bool any = false;

    // the blocks with dropouts in them, and the blocks next to them, whose vectors fill them in
    for (int b=0;b < nb;b++) {
        const int x0 = (b % mv.bw) * B,x1 = std::min(cur.w,x0 + B);
        const int y0 = (b / mv.bw) * B,y1 = std::min(cur.h,y0 + B);
        bool has = false;
        for (int y=y0;y < y1 && !has;y++) {
            const uint8_t *m = &cmask[(size_t)y * cur.w];
            for (int x=x0;x < x1 && !has;x++)
                has = m[x] != 0;
        }
        if (!has)
            continue;
        any = true;
        const int bxp = b % mv.bw,byp = b / mv.bw;
        for (int ny=std::max(0,byp - 1);ny <= std::min(mv.bh - 1,byp + 1);ny++) {
            for (int nx=std::max(0,bxp - 1);nx <= std::min(mv.bw - 1,bxp + 1);nx++)
                near[((size_t)ny * mv.bw) + nx] = 1;
        }
    }
    if (!any)
        return;

    #pragma omp parallel for schedule(dynamic,4)
    for (int b=0;b < nb;b++) {
        if (!near[b])
            continue;
        const int bxp = b % mv.bw,byp = b / mv.bw;
        const int x0 = bxp * B,x1 = std::min(cur.w,x0 + B);
        const int y0 = byp * B,y1 = std::min(cur.h,y0 + B);
        float best = 1e30f,bx = px[b],by = py[b],bc = (float)mp.trunc;
        unsigned int most = 0;

        for (int ny=std::max(0,byp - 1);ny <= std::min(mv.bh - 1,byp + 1);ny++) {
            for (int nx=std::max(0,bxp - 1);nx <= std::min(mv.bw - 1,bxp + 1);nx++) {
                const size_t n = ((size_t)ny * mv.bw) + nx;
                const float dx = px[n],dy = py[n];
                const int idx = (int)floorf(dx + 0.5f),idy = (int)floorf(dy + 0.5f);
                unsigned int count = 0;
                float sum = 0;

                if (n != (size_t)b && dx == px[b] && dy == py[b])
                    continue;

                for (int y=y0;y < y1;y++) {
                    const float *c = cur.row(y);
                    const uint8_t *cm = &cmask[(size_t)y * cur.w];
                    const int ry = y + idy;
                    if (ry < 0 || ry >= ref.h)
                        continue;
                    const uint8_t *rm = &rmask[(size_t)ry * ref.w];
                    for (int x=x0;x < x1;x++) {
                        const int rx = x + idx;
                        if (cm[x] || rx < 0 || rx >= ref.w || rm[rx])
                            continue;
                        sum += std::min(fabsf(c[x] - sample_bilinear(ref,(float)x + dx,(float)y + dy)),(float)mp.trunc);
                        count++;
                    }
                }

                most = std::max(most,count);
                if (count * 4u < (unsigned int)((x1 - x0) * (y1 - y0)))
                    continue;

                const float m = sum / (float)count;
                const float c = m + (mp.lambda * (fabsf(dx) + fabsf(dy)));
                if (c < best) {
                    best = c;
                    bx = dx;
                    by = dy;
                    bc = m;
                }
            }
        }

        if (most * 4u < (unsigned int)((x1 - x0) * (y1 - y0)))
            known[b] = 0;

        mv.vx[b] = bx;
        mv.vy[b] = by;
        mv.cost[b] = bc;
    }

    // The blocks that are almost all dropouts take the vectors of the blocks around them,
    // working in from the edges of a large dropout. Its blocks match as well as those blocks.
    for (unsigned int round=0;round < 64u;round++) {
        std::vector<uint8_t> nk = known;
        bool any = false,left = false;

        for (int b=0;b < nb;b++) {
            if (known[b])
                continue;

            const int bxp = b % mv.bw,byp = b / mv.bw;
            std::vector<float> xs,ys;
            float cs = 0;

            for (int ny=std::max(0,byp - 1);ny <= std::min(mv.bh - 1,byp + 1);ny++) {
                for (int nx=std::max(0,bxp - 1);nx <= std::min(mv.bw - 1,bxp + 1);nx++) {
                    const size_t n = ((size_t)ny * mv.bw) + nx;
                    if (!known[n])
                        continue;
                    xs.push_back(mv.vx[n]);
                    ys.push_back(mv.vy[n]);
                    cs += mv.cost[n];
                }
            }

            if (xs.empty()) {
                left = true;
                continue;
            }

            const size_t m = xs.size() / 2u;
            std::nth_element(xs.begin(),xs.begin() + (ptrdiff_t)m,xs.end());
            std::nth_element(ys.begin(),ys.begin() + (ptrdiff_t)m,ys.end());
            mv.vx[b] = xs[m];
            mv.vy[b] = ys[m];
            mv.cost[b] = cs / (float)xs.size();
            nk[b] = 1;
            any = true;
        }

        known.swap(nk);
        if (!left || !any)
            break;
    }
}

void predict(image_t &out,const image_t &ref,const mvfield_t &mv) {
    out.alloc(ref.w,ref.h);

    #pragma omp parallel for schedule(static)
    for (int y=0;y < out.h;y++) {
        float *d = out.row(y);
        for (int x=0;x < out.w;x++) {
            float dx,dy;
            mv.at((float)x,(float)y,dx,dy);
            d[x] = sample_bilinear(ref,(float)x + dx,(float)y + dy);
        }
    }
}
