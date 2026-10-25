#include <math.h>

#include <algorithm>
#include <vector>

#include "flow.h"

// the smallest image of a pyramid, across or down
static const int PYR_MIN = 24;

void build_pyr(pyr_t &p,const fimage_t &full) {
    p.lv.clear();
    p.lv.push_back(full);
    while (std::min(p.lv.back().w,p.lv.back().h) / 2 >= PYR_MIN) {
        const fimage_t &s = p.lv.back();
        fimage_t d;
        d.alloc(s.w / 2,s.h / 2);
        for (int y=0;y < d.h;y++) {
            const float *r0 = s.row(y * 2),*r1 = s.row((y * 2) + 1);
            float *o = d.row(y);
            for (int x=0;x < d.w;x++)
                o[x] = (r0[x * 2] + r0[(x * 2) + 1] + r1[x * 2] + r1[(x * 2) + 1]) * 0.25f;
        }
        p.lv.push_back(std::move(d));
    }
}

// How badly block (bx,by) of a matches b moved by (vx,vy): the mean difference of the pixels of the block
// and a margin around it, so that blocks overlap, and a block of nothing but sky still has edges to go by;
// each with the mean of its block taken off, so that a fade, or the light changing, does not throw it off.
// If dc is not NULL, it is how much brighter the block is in b.
static float block_cost(const fimage_t &a,const fimage_t &b,int bx,int by,int blk,float vx,float vy,float *dc=NULL) {
    const int m = std::max(1,blk / 4);
    const int x0 = std::max(0,(bx * blk) - m),x1 = std::min(a.w,((bx + 1) * blk) + m);
    const int y0 = std::max(0,(by * blk) - m),y1 = std::min(a.h,((by + 1) * blk) + m);
    const bool whole = vx == floorf(vx) && vy == floorf(vy);
    const int n = std::max(1,(x1 - x0) * (y1 - y0));
    const int ix = (int)vx,iy = (int)vy;
    auto sb = [&](int x,int y) -> float {
        if (whole)
            return b.row(std::min(b.h - 1,std::max(0,y + iy)))[std::min(b.w - 1,std::max(0,x + ix))];
        return b.at((float)x + vx,(float)y + vy);
    };

    float ma = 0,mb = 0;
    for (int y=y0;y < y1;y++) {
        const float *ra = a.row(y);
        for (int x=x0;x < x1;x++) {
            ma += ra[x];
            mb += sb(x,y);
        }
    }
    const float d = (mb - ma) / (float)n;
    if (dc != NULL)
        *dc = d;

    float sum = 0;
    for (int y=y0;y < y1;y++) {
        const float *ra = a.row(y);
        for (int x=x0;x < x1;x++)
            sum += fabsf(ra[x] + d - sb(x,y));
    }
    return sum / (float)n;
}

static float median9(float *v,int n) {
    std::nth_element(v,v + (n / 2),v + n);
    return v[n / 2];
}

// each vector, the median of it and the vectors around it, which takes out the odd one that matched
// something it is not
static void median_filter(field_t &f) {
    const field_t s = f;
    for (int by=0;by < f.bh;by++) {
        for (int bx=0;bx < f.bw;bx++) {
            float xs[9],ys[9];
            int n = 0;
            for (int dy=-1;dy <= 1;dy++) {
                for (int dx=-1;dx <= 1;dx++) {
                    const int x = bx + dx,y = by + dy;
                    if (x < 0 || y < 0 || x >= f.bw || y >= f.bh)
                        continue;
                    xs[n] = s.vx[((size_t)y * f.bw) + x];
                    ys[n] = s.vy[((size_t)y * f.bw) + x];
                    n++;
                }
            }
            f.vx[((size_t)by * f.bw) + bx] = median9(xs,n);
            f.vy[((size_t)by * f.bw) + bx] = median9(ys,n);
        }
    }
}

void estimate_flow(field_t &out,const pyr_t &a,const pyr_t &b,const flow_params_t &fp,const field_t *pred) {
    const int levels = (int)std::min(a.lv.size(),b.lv.size());
    const int blk = fp.block;
    field_t coarse;

    for (int L=levels - 1;L >= 0;L--) {
        const fimage_t &ia = a.lv[(size_t)L],&ib = b.lv[(size_t)L];
        const float scale = (float)(1 << L);
        field_t f;
        f.alloc(ia.w,ia.h,blk);

        // the coarsest level is searched all over, as far as the range, and each finer level from the
        // vectors of the level above, of the block and the blocks around it
        const int full = L == levels - 1 ? (int)ceilf((float)fp.range / scale) + 1 : 0;

#pragma omp parallel for schedule(dynamic,1)
        for (int by=0;by < f.bh;by++) {
            for (int bx=0;bx < f.bw;bx++) {
                const float cx = ((float)bx + 0.5f) * (float)blk,cy = ((float)by + 0.5f) * (float)blk;
                std::vector<float> cand;           // x,y pairs
                float mx = 0,my = 0;

                if (full > 0) {
                    for (int vy=-full;vy <= full;vy++)
                        for (int vx=-full;vx <= full;vx++) {
                            cand.push_back((float)vx);
                            cand.push_back((float)vy);
                        }
                }
                else {
                    float xs[9],ys[9];
                    int n = 0;
                    for (int dy=-1;dy <= 1;dy++) {
                        for (int dx=-1;dx <= 1;dx++) {
                            float vx,vy;
                            coarse.at((cx + (float)(dx * blk)) * 0.5f,(cy + (float)(dy * blk)) * 0.5f,vx,vy);
                            xs[n] = vx * 2.0f;
                            ys[n] = vy * 2.0f;
                            cand.push_back(floorf(xs[n] + 0.5f));
                            cand.push_back(floorf(ys[n] + 0.5f));
                            n++;
                        }
                    }
                    mx = median9(xs,n);
                    my = median9(ys,n);
                    cand.push_back(0.0f);
                    cand.push_back(0.0f);
                }
                float px = 0,py = 0;
                if (pred != NULL && !pred->empty()) {
                    pred->at(cx * scale,cy * scale,px,py);
                    px /= scale;
                    py /= scale;
                    cand.push_back(floorf(px + 0.5f));
                    cand.push_back(floorf(py + 0.5f));
                }

                auto cost = [&](float vx,float vy) -> float {
                    float c = block_cost(ia,ib,bx,by,blk,vx,vy);
                    if (full == 0)
                        c += fp.smooth * (fabsf(vx - mx) + fabsf(vy - my));
                    if (pred != NULL && !pred->empty())
                        c += fp.predict * (fabsf(vx - px) + fabsf(vy - py));
                    return c;
                };

                float bxv = 0,byv = 0,bc = 1e30f;
                for (size_t i=0;i < cand.size();i += 2) {
                    const float c = cost(cand[i],cand[i + 1]);
                    if (c < bc) {
                        bc = c;
                        bxv = cand[i];
                        byv = cand[i + 1];
                    }
                }

                // a pixel at a time around the best, and at the full size, a half and a quarter of one
                for (int it=0;it < 4;it++) {
                    bool moved = false;
                    for (int d=0;d < 8;d++) {
                        static const int ox[8] = { -1,1,0,0,-1,1,-1,1 },oy[8] = { 0,0,-1,1,-1,-1,1,1 };
                        const float c = cost(bxv + (float)ox[d],byv + (float)oy[d]);
                        if (c < bc) {
                            bc = c;
                            bxv += (float)ox[d];
                            byv += (float)oy[d];
                            moved = true;
                        }
                    }
                    if (!moved)
                        break;
                }
                if (L == 0 && fp.subpel) {
                    for (float step=0.5f;step >= 0.25f;step *= 0.5f) {
                        float nx = bxv,ny = byv;
                        for (int dy=-1;dy <= 1;dy++) {
                            for (int dx=-1;dx <= 1;dx++) {
                                if (dx == 0 && dy == 0)
                                    continue;
                                const float c = cost(bxv + (step * (float)dx),byv + (step * (float)dy));
                                if (c < bc) {
                                    bc = c;
                                    nx = bxv + (step * (float)dx);
                                    ny = byv + (step * (float)dy);
                                }
                            }
                        }
                        bxv = nx;
                        byv = ny;
                    }
                }

                f.vx[((size_t)by * f.bw) + bx] = bxv;
                f.vy[((size_t)by * f.bw) + bx] = byv;
            }
        }

        median_filter(f);
        coarse = std::move(f);
    }

    // how well each block matches where its vector puts it
    out = coarse;
#pragma omp parallel for schedule(dynamic,1)
    for (int by=0;by < out.bh;by++) {
        for (int bx=0;bx < out.bw;bx++) {
            const size_t i = ((size_t)by * out.bw) + bx;
            out.cost[i] = block_cost(a.lv[0],b.lv[0],bx,by,blk,out.vx[i],out.vy[i],&out.dc[i]);
        }
    }
}
