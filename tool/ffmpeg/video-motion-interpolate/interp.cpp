#include <math.h>

#include <algorithm>
#include <vector>

#include "interp.h"

// how many levels a pixel of a and of b can differ, along a path, and still be taken as the same thing
static const float MATCH_SIGMA = 10.0f;

// how far apart, in pixels, the vector from a to b and back from b to a can end up, plus this fraction of
// how far it moved, and still be the same thing seen in both; farther, it is in only one of them: covered
// by something moving in front of it, or uncovered from behind it
static const float CONSISTENT_PIXELS = 1.0f;
static const float CONSISTENT_FRACTION = 0.15f;

// a path that is not seen in both frames counts for this much, against a perfect match of one that is
static const float ONE_SIDED = 0.15f;

// The path of a pixel from where it is in one frame (0) to where it is in the next (v), a fraction t of the
// way, as a cubic: going at the start as it came into the first frame and went on to the next, and at the
// end as it went on and then went on again to the frame after, each averaged, which follows the motion as
// it speeds up, slows down and turns, not only in a straight line. in and out are how it moved in the time
// before and after (already made as long as the time between the two frames), or v itself if not known.
// They are kept near v, so that a vector that matched something wrong does not throw the path far off.
static void path(float t,float vx,float vy,float inx,float iny,float outx,float outy,float &ox,float &oy) {
    const float lim = std::max(2.0f,0.5f * sqrtf((vx * vx) + (vy * vy)));
    auto keep = [&](float &px,float &py) {
        const float dx = px - vx,dy = py - vy,d = sqrtf((dx * dx) + (dy * dy));
        if (d > lim) {
            px = vx + (dx * lim / d);
            py = vy + (dy * lim / d);
        }
    };
    float tax = 0.5f * (vx + inx),tay = 0.5f * (vy + iny);
    float tbx = 0.5f * (vx + outx),tby = 0.5f * (vy + outy);
    keep(tax,tay);
    keep(tbx,tby);

    const float t2 = t * t,t3 = t2 * t;
    const float h10 = t3 - (2.0f * t2) + t,h01 = (-2.0f * t3) + (3.0f * t2),h11 = t3 - t2;
    ox = (h10 * tax) + (h01 * vx) + (h11 * tbx);
    oy = (h10 * tay) + (h01 * vy) + (h11 * tby);
}

void interpolate(std::vector<fimage_t> &out,const between_t &bt,float t) {
    const frame_t &A = *bt.a,&B = *bt.b;
    const size_t np = A.planes.size();
    const int W = A.planes[0].w,H = A.planes[0].h;
    const float s = bt.scale;
    const float ka = bt.a_prev != NULL && bt.dt_prev > 0 ? (float)(bt.dt / bt.dt_prev) : 0.0f;
    const float kb = bt.b_next != NULL && bt.dt_next > 0 ? (float)(bt.dt / bt.dt_next) : 0.0f;

    out.resize(np);
    for (size_t p=0;p < np;p++)
        out[p].alloc(W,H);

    // how it came into a, at a point of a, and went on from b, at a point of b, over the time between them
    auto into_a = [&](float x,float y,float vx,float vy,float &ix,float &iy) {
        if (bt.a_prev != NULL) {
            float dx,dy;
            bt.a_prev->at(x,y,dx,dy);
            ix = -dx * ka;
            iy = -dy * ka;
        }
        else {
            ix = vx;
            iy = vy;
        }
    };
    auto from_b = [&](float x,float y,float vx,float vy,float &ox,float &oy) {
        if (bt.b_next != NULL) {
            float dx,dy;
            bt.b_next->at(x,y,dx,dy);
            ox = dx * kb;
            oy = dy * kb;
        }
        else {
            ox = vx;
            oy = vy;
        }
    };

#pragma omp parallel for schedule(dynamic,4)
    for (int Y=0;Y < H;Y++) {
        for (int X=0;X < W;X++) {
            // the point, at the size of the luma, that the motion is measured at
            const float x = (((float)X + 0.5f) / s) - 0.5f,y = (((float)Y + 0.5f) / s) - 0.5f;

            // From a: the point of a whose path goes through here at t. The path depends on where it starts,
            // so it is found a few times over, each from the last.
            float ax = x,ay = y,vx = 0,vy = 0;
            for (int it=0;it < 4;it++) {
                float ix,iy,ox,oy,px,py;
                bt.ab->at(ax,ay,vx,vy);
                into_a(ax,ay,vx,vy,ix,iy);
                from_b(ax + vx,ay + vy,vx,vy,ox,oy);
                path(t,vx,vy,ix,iy,ox,oy,px,py);
                ax = x - px;
                ay = y - py;
            }
            bt.ab->at(ax,ay,vx,vy);

            // From b, going back in time: the point of b whose path back to a goes through here at t
            float bx = x,by = y,wx = 0,wy = 0;
            for (int it=0;it < 4;it++) {
                float ix,iy,ox,oy,px,py;
                bt.ba->at(bx,by,wx,wy);
                // backwards, b is where it comes from b's frame after, and a is where it goes on to a's frame before
                from_b(bx,by,-wx,-wy,ix,iy);
                ix = -ix;
                iy = -iy;
                into_a(bx + wx,by + wy,-wx,-wy,ox,oy);
                ox = -ox;
                oy = -oy;
                path(1.0f - t,wx,wy,ix,iy,ox,oy,px,py);
                bx = x - px;
                by = y - py;
            }
            bt.ba->at(bx,by,wx,wy);

            // whether each is seen in both frames: going there and back ends up where it started
            float rx,ry;
            bt.ba->at(ax + vx,ay + vy,rx,ry);
            const float cf = fabsf(vx + rx) + fabsf(vy + ry);
            const bool both_f = cf <= CONSISTENT_PIXELS + (CONSISTENT_FRACTION * (fabsf(vx) + fabsf(vy)));
            bt.ab->at(bx + wx,by + wy,rx,ry);
            const float cb = fabsf(wx + rx) + fabsf(wy + ry);
            const bool both_b = cb <= CONSISTENT_PIXELS + (CONSISTENT_FRACTION * (fabsf(wx) + fabsf(wy)));

            // How much each counts: one seen in both frames by how alike it is in them, and is the two
            // blended, the nearer in time the more; one seen only in its own frame, covered after a or
            // uncovered before b, a little, the more the nearer its frame is, and is that frame's alone.
            float wf,wb;
            // (how alike, once the brightness of the block it is in is made the same: a fade or the light
            // changing makes each pixel brighter or darker, and it is still the same thing)
            if (both_f)
                wf = expf(-fabsf(A.luma.at(ax,ay) + bt.ab->dc_at(ax,ay) - B.luma.at(ax + vx,ay + vy)) / MATCH_SIGMA);
            else
                wf = ONE_SIDED * (1.0f - t);
            if (both_b)
                wb = expf(-fabsf(B.luma.at(bx,by) + bt.ba->dc_at(bx,by) - A.luma.at(bx + wx,by + wy)) / MATCH_SIGMA);
            else
                wb = ONE_SIDED * t;
            const float sum = wf + wb;
            if (sum < 1e-6f) {
                wf = 1.0f - t;
                wb = t;
            }
            else {
                wf /= sum;
                wb /= sum;
            }

            // the points in the upscaled planes
            auto up = [&](float v) -> float { return ((v + 0.5f) * s) - 0.5f; };
            const float fa_x = up(ax),fa_y = up(ay),fb_x = up(ax + vx),fb_y = up(ay + vy);
            const float ba_x = up(bx + wx),ba_y = up(by + wy),bb_x = up(bx),bb_y = up(by);
            const float fwa = both_f ? 1.0f - t : 1.0f,fwb = both_f ? t : 0.0f;
            const float bwa = both_b ? 1.0f - t : 0.0f,bwb = both_b ? t : 1.0f;

            for (size_t p=0;p < np;p++) {
                const fimage_t &pa = A.planes[p],&pb = B.planes[p];
                float vf = fwa * pa.at(fa_x,fa_y);
                if (fwb > 0) vf += fwb * pb.at(fb_x,fb_y);
                float vb = bwb * pb.at(bb_x,bb_y);
                if (bwa > 0) vb += bwa * pa.at(ba_x,ba_y);
                out[p].row(Y)[X] = (wf * vf) + (wb * vb);
            }
        }
    }
}
