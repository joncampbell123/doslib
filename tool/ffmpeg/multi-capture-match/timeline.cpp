#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include <algorithm>
#include <vector>

#include "timeline.h"

// a thumbnail with less standard deviation than this, in levels, is blank (as match_frames takes it):
// the screen of a VCR with no tape playing
static const double BLANK = 4.0;

static bool thumb_blank(const uint8_t *t) {
    double sum = 0,sum2 = 0;
    for (int k=0;k < THUMB_SIZE;k++) {
        sum += t[k];
        sum2 += (double)t[k] * t[k];
    }
    const double mean = sum / THUMB_SIZE;
    return ((sum2 / THUMB_SIZE) - (mean * mean)) < BLANK * BLANK;
}

// A frame the same as the frame before it, as a capture card puts in for a frame it dropped. It is not that
// frame, and it is not the frame before it again either.
static bool is_repeat(const capture_t &c,size_t i) {
    if (i == 0)
        return false;
    const uint8_t *a = c.thumbs.thumb(i),*b = c.thumbs.thumb(i - 1u);
    unsigned int sum = 0;
    for (int k=0;k < THUMB_SIZE;k++)
        sum += (unsigned int)abs((int)a[k] - (int)b[k]);
    return sum * 10u < (unsigned int)THUMB_SIZE && !thumb_blank(a);
}

// Which slot of the timeline each of frames f0 to f1 of capture c is, of slots s0 to s1, by its pictures, its
// sound, or both. m[i] is of frame f0 + i, and its slot is of the whole timeline.
static void match_capture(std::vector<frame_match_t> &m,const std::vector<slot_t> &tl,const std::vector<capture_t> &caps,unsigned int c,
    match_by_t by,const match_params_t &mp,size_t f0,size_t f1,size_t s0,size_t s1) {
    const capture_t &cap = caps[c];
    const double step = 1.0 / std::max(1.0,cap.frame_rate());
    const size_t n = f1 - f0,slots = s1 - s0;

    // The slots, with blank frames before and after them: match_frames finds where a window of frames is
    // only where all of it is in what it is matched to, and a capture may start before the timeline does,
    // or end after it.
    const size_t pad = std::min((size_t)std::max(mp.window,64),std::max(n,(size_t)16));
    auto timeline_thumbs = [&](thumbs_t &t,bool audio) {
        t = thumbs_t();
        t.time.resize(slots + (2u * pad));
        t.v.assign(t.time.size() * THUMB_SIZE,0);
        for (size_t s=0;s < t.time.size();s++)
            t.time[s] = (double)s * step;
        for (size_t s=0;s < slots;s++) {
            const cand_t &k = tl[s0 + s].c[tl[s0 + s].src];
            uint8_t *d = &t.v[(s + pad) * THUMB_SIZE];
            if (audio)
                audio_thumb(d,caps[k.cap],(size_t)k.frame);
            else
                memcpy(d,caps[k.cap].thumbs.thumb((size_t)k.frame),THUMB_SIZE);
        }
    };
    auto capture_thumbs = [&](thumbs_t &t,bool audio) {
        t = thumbs_t();
        t.time.assign(cap.thumbs.time.begin() + (ptrdiff_t)f0,cap.thumbs.time.begin() + (ptrdiff_t)f1);
        t.v.resize(n * THUMB_SIZE);
        for (size_t i=0;i < n;i++) {
            if (audio)
                audio_thumb(&t.v[i * THUMB_SIZE],cap,f0 + i);
            else
                memcpy(&t.v[i * THUMB_SIZE],cap.thumbs.thumb(f0 + i),THUMB_SIZE);
        }
    };
    auto unpad = [&](std::vector<frame_match_t> &mm) {
        for (frame_match_t &f : mm) {
            if (f.frame >= 0) {
                f.frame -= (int64_t)pad;
                if (f.frame < 0 || f.frame >= (int64_t)slots)
                    f = frame_match_t();
                else
                    f.frame += (int64_t)s0;
            }
        }
    };

    m.assign(n,frame_match_t());
    if (n == 0 || slots == 0)
        return;
    if (by & MATCH_VIDEO) {
        thumbs_t t,in;
        timeline_thumbs(t,false);
        capture_thumbs(in,false);
        match_frames(m,in,t,mp);
        unpad(m);
    }
    if (by & MATCH_AUDIO) {
        thumbs_t t,in;
        std::vector<frame_match_t> ma;
        timeline_thumbs(t,true);
        capture_thumbs(in,true);
        match_frames(ma,in,t,mp);
        unpad(ma);
        // the pictures go to the frame, the sound only to about the frame, so the sound only where
        // the pictures do not match
        for (size_t i=0;i < m.size();i++) {
            if (m[i].frame < 0)
                m[i] = ma[i];
        }
    }
}

// how alike two thumbnails are, -1 to 1, as match_frames scores them: the correlation of their samples,
// or -1 if either is blank
static float correlation(const uint8_t *a,const uint8_t *b) {
    double sa = 0,sb = 0,saa = 0,sbb = 0,sab = 0;
    for (int k=0;k < THUMB_SIZE;k++) {
        sa += a[k];
        sb += b[k];
        saa += (double)a[k] * a[k];
        sbb += (double)b[k] * b[k];
        sab += (double)a[k] * b[k];
    }
    const double ma = sa / THUMB_SIZE,mb = sb / THUMB_SIZE;
    const double va = (saa / THUMB_SIZE) - (ma * ma),vb = (sbb / THUMB_SIZE) - (mb * mb);
    if (va < BLANK * BLANK || vb < BLANK * BLANK)
        return -1.0f;
    return (float)(((sab / THUMB_SIZE) - (ma * mb)) / sqrt(va * vb));
}

// The frames of a capture that matched, and their slots. Two frames one after the other that matched the
// same slot are not both that frame (unless the second is a repeat of the first, which is not a frame at all):
// a frame much like the one before it, as in a slow pan, can match the slot of the frame before it as
// well as its own, which the timeline may not have. The one that matched less well is taken as not matched.
//
// And two frames a few apart that matched slots the other way around, the later frame an earlier slot, are
// neither of them those frames: they are frames the timeline does not have, each matching the slots of the
// frames either side of where they go.
static void matched_frames(std::vector<std::pair<int64_t,int64_t> > &M,std::vector<frame_match_t> &m,const capture_t &cap) {
    for (size_t i=1;i < m.size();i++) {
        if (m[i].frame >= 0 && m[i].frame == m[i - 1u].frame && !is_repeat(cap,i)) {
            if (m[i].score > m[i - 1u].score)
                m[i - 1u] = frame_match_t();
            else
                m[i] = frame_match_t();
        }
    }
    for (bool again=true;again;) {
        again = false;
        size_t p = SIZE_MAX;
        for (size_t i=0;i < m.size();i++) {
            if (m[i].frame < 0)
                continue;
            if (p != SIZE_MAX && i - p <= 4u && m[i].frame <= m[p].frame && !is_repeat(cap,i)) {
                m[i] = frame_match_t();
                m[p] = frame_match_t();
                again = true;
                p = SIZE_MAX;
                continue;
            }
            p = i;
        }
    }
    M.clear();
    for (size_t i=0;i < m.size();i++) {
        if (m[i].frame >= 0)
            M.push_back(std::make_pair((int64_t)i,m[i].frame));
    }
}

// a run of frames of a capture to put in the timeline after slot after (-1 for at the start)
struct insert_t {
    int64_t                     after = -1;
    size_t                      first = 0,count = 0;
};

bool build_timeline(std::vector<slot_t> &tl,const std::vector<capture_t> &caps,unsigned int ref,const timeline_params_t &tp) {
    match_params_t mp;
    mp.min_score = tp.min_score;
    mp.window = tp.window;
    unsigned int used = 1;

    tl.clear();
    tl.resize(caps[ref].frames());
    for (size_t i=0;i < tl.size();i++) {
        cand_t k;
        k.cap = ref;
        k.frame = (int64_t)i;
        k.score = 1.0f;
        k.matched = true;
        tl[i].c.push_back(k);
    }

    for (unsigned int c=0;c < caps.size();c++) {
        if (c == ref)
            continue;

        const capture_t &cap = caps[c];
        match_by_t by = tp.by;
        if ((by & MATCH_AUDIO) && cap.audio.empty()) {
            fprintf(stderr,"%s has no audio: matching it by the pictures\n",cap.path.c_str());
            by = MATCH_VIDEO;
        }
        if (!tp.quiet)
            fprintf(stderr,"Matching %s to the timeline\n",cap.path.c_str());

        const size_t n = cap.frames();
        const int64_t slots = (int64_t)tl.size();
        std::vector<frame_match_t> m;
        std::vector<std::pair<int64_t,int64_t> > M;    // frames that matched, and their slots
        // How alike frame i is to slot sl, to the frame: by the pictures, which tell one frame from the next,
        // where they are alike; or else, if matching by the sound, by the sound, which is a thumbnail of a third
        // of a second of it, and tells where a frame is only to about the frame.
        std::vector<uint8_t> ta(THUMB_SIZE),tb(THUMB_SIZE);
        auto fine = [&](size_t i,int64_t sl) -> float {
            const cand_t &k = tl[(size_t)sl].c[tl[(size_t)sl].src];
            const float v = correlation(cap.thumbs.thumb(i),caps[k.cap].thumbs.thumb((size_t)k.frame));
            if (!(by & MATCH_AUDIO) || v >= 0.8f)
                return v;
            audio_thumb(&ta[0],cap,i);
            audio_thumb(&tb[0],caps[k.cap],(size_t)k.frame);
            return correlation(&ta[0],&tb[0]);
        };
        // the frames that matched, scored so
        auto rescore = [&](std::vector<frame_match_t> &mm,size_t f0) {
            if (!(by & MATCH_AUDIO))
                return;
            for (size_t i=0;i < mm.size();i++) {
                if (mm[i].frame >= 0)
                    mm[i].score = fine(f0 + i,mm[i].frame);
            }
        };

        // How far each frame that matched is from its slot, again, through each stretch of them: match_frames
        // takes a frame that is as like its slot as the slots either side of it, to within 0.002, as frames in
        // a slow zoom or pan or with dropouts are, and goes on from the frames around it, which can put some of
        // them a frame out. That is near enough for filling in a dropout, but not for a cut from one capture to
        // another. A capture goes on a frame for each frame of the timeline, and goes one more or less only
        // where a capture card or a VCR dropped one, and then stays so: so each frame takes the offset that is
        // best for the whole stretch, with a cost for each change of it that only a run of frames that are
        // better for it pays for, not a few noisy ones.
        auto settle = [&](void) {
            const int R = 3;
            const double change = 0.05;
            size_t i = 0;
            while (i < n) {
                if (m[i].frame < 0 || is_repeat(cap,i)) {
                    i++;
                    continue;
                }
                // the stretch: frames that matched within R of the offset of the first, with no more than 30
                // frames between them
                const int64_t lag0 = m[i].frame - (int64_t)i;
                std::vector<size_t> F;
                size_t last = i;
                for (size_t k=i;k < n && k - last <= 30u;k++) {
                    if (m[k].frame < 0 || is_repeat(cap,k))
                        continue;
                    if (llabs((m[k].frame - (int64_t)k) - lag0) > R)
                        break;
                    F.push_back(k);
                    last = k;
                }
                const size_t nf = F.size();
                std::vector<double> cost((size_t)(2 * R + 1),0.0),nc((size_t)(2 * R + 1));
                std::vector<std::vector<signed char> > from(nf,std::vector<signed char>((size_t)(2 * R + 1),0));
                for (size_t k=0;k < nf;k++) {
                    double bp = 1e300;
                    int bd = 0;
                    for (int d=0;d <= 2 * R;d++) {
                        if (cost[(size_t)d] < bp) {
                            bp = cost[(size_t)d];
                            bd = d;
                        }
                    }
                    for (int d=0;d <= 2 * R;d++) {
                        const int64_t sl = (int64_t)F[k] + lag0 + d - R;
                        const double e = (sl >= 0 && sl < slots) ? fine(F[k],sl) : -1.0;
                        double v = cost[(size_t)d];
                        int f = d;
                        if (k > 0 && bp + change < v) {
                            v = bp + change;
                            f = bd;
                        }
                        nc[(size_t)d] = v - e;
                        from[k][(size_t)d] = (signed char)f;
                    }
                    cost.swap(nc);
                }
                int d = (int)(std::min_element(cost.begin(),cost.end()) - cost.begin());
                for (size_t k=nf;k-- > 0;) {
                    const size_t f = F[k];
                    m[f].frame = (int64_t)f + lag0 + d - R;
                    m[f].score = fine(f,m[f].frame);
                    d = from[k][(size_t)d];
                }
                i = F.back() + 1u;
            }
            matched_frames(M,m,cap);
        };

        match_capture(m,tl,caps,c,by,mp,0,n,0,tl.size());
        rescore(m,0);
        settle();

        // Each run of frames between those that matched, frame by frame, going on from the frames either side
        // of it, as far as each goes on as it did: as match_frames goes on from the window before, which is all
        // there is to go by where the picture changes evenly, as in a pan, and the pattern of change is flat.
        auto go_on = [&](void) {
            // as match_frames takes a frame: as alike as min_score, and the best of the slots either side of it
            auto try_frame = [&](int64_t i,int64_t sl) -> bool {
                const float sc = fine((size_t)i,sl);
                if (sc < tp.min_score)
                    return false;
                for (int64_t e=-2;e <= 2;e++) {
                    if (e != 0 && sl + e >= 0 && sl + e < slots && fine((size_t)i,sl + e) > sc + 0.002f)
                        return false;
                }
                if (m[(size_t)i].frame < 0 || sc > m[(size_t)i].score) {
                    m[(size_t)i].frame = sl;
                    m[(size_t)i].score = sc;
                }
                return true;
            };
            const std::vector<std::pair<int64_t,int64_t> > M0 = M;
            for (size_t r=0;r <= M0.size();r++) {
                const int64_t a = r > 0 ? M0[r - 1u].first : -1,ja = r > 0 ? M0[r - 1u].second : -1;
                const int64_t b = r < M0.size() ? M0[r].first : (int64_t)n,jb = r < M0.size() ? M0[r].second : slots;
                // a few frames that do not match, as ones too damaged to, do not stop it
                int fails = 0;
                for (int64_t i=a + 1;r > 0 && i < b && fails <= 3;i++) {
                    const int64_t sl = i + (ja - a);
                    if (sl >= slots || (jb > ja && sl >= jb))
                        break;
                    fails = try_frame(i,sl) ? 0 : fails + 1;
                }
                fails = 0;
                for (int64_t i=b - 1;r < M0.size() && i > a && fails <= 3;i--) {
                    const int64_t sl = i + (jb - b);
                    if (sl < 0 || (jb > ja && sl <= ja))
                        break;
                    fails = try_frame(i,sl) ? 0 : fails + 1;
                }
            }
            matched_frames(M,m,cap);
        };
        go_on();

        // Each run of frames between those that matched, matched again by itself, to the slots between theirs:
        // the windows match_frames matches at first are a few seconds long, and those that go over where a
        // capture skipped or stopped recording match neither side of it. With only the run, they are shorter.
        for (int pass=0;pass < 2 && !M.empty();pass++) {
            bool more = false;
            for (size_t r=0;r <= M.size();r++) {
                const int64_t a = r > 0 ? M[r - 1u].first : -1,ja = r > 0 ? M[r - 1u].second : -1;
                const int64_t b = r < M.size() ? M[r].first : (int64_t)n,jb = r < M.size() ? M[r].second : slots;
                if (b - a - 1 < 8 || jb - ja - 1 < 8)
                    continue;
                std::vector<frame_match_t> sub;
                match_capture(sub,tl,caps,c,by,mp,(size_t)(a + 1),(size_t)b,(size_t)(ja + 1),(size_t)jb);
                rescore(sub,(size_t)(a + 1));
                // MCM_DEBUG in the environment says how each run and each frame matched, for looking into it
                if (getenv("MCM_DEBUG") != NULL) {
                    size_t got = 0;
                    for (const frame_match_t &f : sub) got += f.frame >= 0 ? 1u : 0u;
                    fprintf(stderr,"refine %u pass %d frames %lld-%lld slots %lld-%lld: %zu matched\n",c,pass,(long long)(a + 1),(long long)(b - 1),(long long)(ja + 1),(long long)(jb - 1),got);
                }
                for (size_t i=0;i < sub.size();i++) {
                    if (sub[i].frame >= 0) {
                        m[(size_t)(a + 1) + i] = sub[i];
                        more = true;
                    }
                }
            }
            if (!more)
                break;
            settle();
            go_on();
        }
        if (getenv("MCM_DEBUG") != NULL) {
            for (size_t i=0;i < m.size();i++) {
                const int64_t j = m[i].frame;
                fprintf(stderr,"match %u %zu %lld %.4f",c,i,(long long)j,m[i].score);
                if (j >= 0) fprintf(stderr," = %u:%lld",tl[(size_t)j].c[tl[(size_t)j].src].cap,(long long)tl[(size_t)j].c[tl[(size_t)j].src].frame);
                fprintf(stderr,"\n");
            }
        }

        std::vector<int64_t> slot_of(n,-1);
        std::vector<char> matched(n,0),exact(n,0);  // exact: not matched, but in a run as long as the slots it is in
        for (const auto &x : M) {
            slot_of[(size_t)x.first] = x.second;
            matched[(size_t)x.first] = 1;
        }
        if (M.empty()) {
            fprintf(stderr,"%s does not match the others at all, and is left out\n",cap.path.c_str());
            continue;
        }
        used++;

        // the frames between those that matched: too damaged to match, if there are about as many as there are
        // slots between theirs, or else what the timeline does not have
        std::vector<insert_t> ins;
        size_t inserted = 0;
        for (size_t r=0;r <= M.size();r++) {
            const bool real_a = r > 0,real_b = r < M.size();
            const int64_t a = real_a ? M[r - 1u].first : -1,ja = real_a ? M[r - 1u].second : -1;
            const int64_t b = real_b ? M[r].first : (int64_t)n,jb = real_b ? M[r].second : slots;
            const int64_t run = b - a - 1;
            const int64_t gap = std::max((int64_t)0,jb - ja - 1);
            if (run <= 0)
                continue;

            // frames between two that matched slots one after the other are all what the timeline does not have
            if (run > gap + tp.insert_min || (real_a && real_b && jb == ja + 1)) {
                // The slots between, if there are any, are some of the run: some of them the start of it, and the
                // rest the end of it, as where the timeline is of a capture that stopped recording a while, and
                // this one did not, whichever split of them they are most like. The rest of it, between, is what
                // the timeline does not have.
                // Each frame of the start or end of it that is taken as a slot is worth how alike they are, less
                // a half: a frame not as alike as that is taken to be one the timeline does not have.
                int64_t head = 0,tail = 0;
                const int64_t most = std::min(gap,run);
                if (most > 0 && jb > ja) {
                    std::vector<double> hs((size_t)most + 1u,0.0),ts((size_t)most + 1u,0.0);
                    for (int64_t k=0;k < most;k++) {
                        hs[(size_t)k + 1u] = hs[(size_t)k] + (real_a ? fine((size_t)(a + 1 + k),ja + 1 + k) - 0.5 : -1.0);
                        ts[(size_t)k + 1u] = ts[(size_t)k] + (real_b ? fine((size_t)(b - 1 - k),jb - 1 - k) - 0.5 : -1.0);
                    }
                    // the best end for each start: the most of ts up to each length
                    std::vector<int64_t> tbest((size_t)most + 1u,0);
                    for (int64_t t=1;t <= most;t++)
                        tbest[(size_t)t] = ts[(size_t)t] > ts[(size_t)tbest[(size_t)t - 1u]] ? t : tbest[(size_t)t - 1u];
                    double best = 0;
                    for (int64_t h=0;h <= most;h++) {
                        const int64_t t = tbest[(size_t)(most - h)];
                        const double v = hs[(size_t)h] + ts[(size_t)t];
                        if (v > best) {
                            best = v;
                            head = h;
                            tail = t;
                        }
                    }
                }
                for (int64_t k=0;k < head;k++) {
                    slot_of[(size_t)(a + 1 + k)] = ja + 1 + k;
                    exact[(size_t)(a + 1 + k)] = 1;
                }
                for (int64_t k=0;k < tail;k++) {
                    slot_of[(size_t)(b - 1 - k)] = jb - 1 - k;
                    exact[(size_t)(b - 1 - k)] = 1;
                }

                // what is not blank of the rest
                int64_t f0 = a + 1 + head,f1 = b - tail;
                while (f0 < f1 && thumb_blank(cap.thumbs.thumb((size_t)f0))) f0++;
                while (f1 > f0 && thumb_blank(cap.thumbs.thumb((size_t)(f1 - 1)))) f1--;
                if (f1 - f0 > tp.insert_min || (real_a && real_b && (jb == ja + 1 || head + tail > 0) && f1 > f0)) {
                    insert_t in;
                    in.after = ja + head;
                    in.first = (size_t)f0;
                    in.count = (size_t)(f1 - f0);
                    ins.push_back(in);
                    inserted += in.count;
                }
                continue;
            }

            // the first half from the frame before it, the rest from the frame after it
            for (int64_t i=a + 1;i < b;i++) {
                int64_t s = -1;
                if (real_a && (!real_b || (i - a) <= (run + 1) / 2))
                    s = ja + (i - a);
                else if (real_b)
                    s = jb - (b - i);
                if (s < 0 || s >= slots || (jb > ja && (s <= ja || s >= jb)))
                    continue;
                slot_of[(size_t)i] = s;
                exact[(size_t)i] = (real_a && real_b && run == gap) ? 1 : 0;
            }
        }

        // the frames of the capture, as candidates of their slots, and the runs put in the timeline, in the
        // order they go in it (a capture that goes back, as one of a tape played again, has them out of order)
        std::stable_sort(ins.begin(),ins.end(),[](const insert_t &x,const insert_t &y) { return x.after < y.after; });
        // how much less like the slots either side each frame that matched is than its own
        std::vector<float> margin(n,0.0f);
        for (size_t i=0;i < n;i++) {
            if (!matched[i])
                continue;
            const int64_t sl = slot_of[i];
            float other = -1.0f;
            if (sl > 0) other = std::max(other,fine(i,sl - 1));
            if (sl + 1 < slots) other = std::max(other,fine(i,sl + 1));
            margin[i] = other > -1.0f ? fine(i,sl) - other : 0.0f;
        }
        std::vector<std::vector<size_t> > at(tl.size());
        for (size_t i=0;i < n;i++) {
            if (slot_of[i] >= 0)
                at[(size_t)slot_of[i]].push_back(i);
        }
        std::vector<slot_t> nt;
        nt.reserve(tl.size() + inserted);
        size_t ii = 0;
        for (int64_t s=-1;s < slots;s++) {
            if (s >= 0) {
                slot_t &sl = tl[(size_t)s];
                for (size_t i : at[(size_t)s]) {
                    // a frame with no picture, though its sound matched, is no use
                    if (thumb_blank(cap.thumbs.thumb(i)) && !tp.keep_blank)
                        continue;
                    cand_t k;
                    k.cap = c;
                    k.frame = (int64_t)i;
                    k.score = matched[i] ? m[i].score : (exact[i] ? 0.5f : 0.0f);
                    k.matched = matched[i] != 0;
                    k.margin = margin[i];
                    sl.c.push_back(k);
                }
                nt.push_back(std::move(sl));
            }
            for (;ii < ins.size() && ins[ii].after == s;ii++) {
                for (size_t f=ins[ii].first;f < ins[ii].first + ins[ii].count;f++) {
                    if (is_repeat(cap,f))
                        continue;
                    slot_t sl;
                    cand_t k;
                    k.cap = c;
                    k.frame = (int64_t)f;
                    k.score = 1.0f;
                    k.matched = true;
                    sl.c.push_back(k);
                    nt.push_back(std::move(sl));
                }
            }
        }
        tl.swap(nt);

        // the timeline is matched to the least damaged of the frames that matched it, from now on
        for (slot_t &sl : tl) {
            const cand_t &k0 = sl.c[sl.src];
            unsigned int best = caps[k0.cap].damage[(size_t)k0.frame];
            for (size_t i=0;i < sl.c.size();i++) {
                const cand_t &k = sl.c[i];
                const unsigned int d = caps[k.cap].damage[(size_t)k.frame];
                if (k.matched && d + 8u < best) {
                    best = d;
                    sl.src = (unsigned int)i;
                }
            }
        }

        if (!tp.quiet) {
            fprintf(stderr,"  %zu of its %zu frames matched (%.1f%%)",M.size(),n,(100.0 * (double)M.size()) / (double)n);
            if (inserted != 0)
                fprintf(stderr,", and %zu frames that the timeline did not have were put in it",inserted);
            fprintf(stderr,"\n");
        }
    }

    if (!tp.keep_blank) {
        std::vector<slot_t> nt;
        nt.reserve(tl.size());
        for (slot_t &sl : tl) {
            bool blank = true;
            for (const cand_t &k : sl.c)
                blank = blank && thumb_blank(caps[k.cap].thumbs.thumb((size_t)k.frame));
            if (!blank)
                nt.push_back(std::move(sl));
        }
        tl.swap(nt);
    }

    if (used < 2u && caps.size() > 1u) {
        fprintf(stderr,"None of the captures match each other\n");
        return false;
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////

// whether frame b of capture cap comes straight after frame a of it
static bool follows(const capture_t &cap,int64_t a,int64_t b) {
    return b == a + 1 && cap.position((size_t)b) == cap.position((size_t)a) + 1;
}

void pick_best(std::vector<int> &choice,const std::vector<slot_t> &tl,const std::vector<capture_t> &caps,const pick_params_t &pp) {
    // the noise of each capture, over a second or so, as it is how good the copy is, not of each frame
    std::vector<std::vector<float> > noise(caps.size());
    for (size_t c=0;c < caps.size();c++) {
        const std::vector<float> &nz = caps[c].noise;
        const size_t n = nz.size();
        const size_t r = 30;
        std::vector<double> cs(n + 1u,0.0);
        for (size_t i=0;i < n;i++)
            cs[i + 1u] = cs[i] + nz[i];
        noise[c].resize(n);
        for (size_t i=0;i < n;i++) {
            const size_t lo = i >= r ? i - r : 0,hi = std::min(n,i + r + 1u);
            noise[c][i] = (float)((cs[hi] - cs[lo]) / (double)(hi - lo));
        }
    }

    auto cost = [&](const cand_t &k) -> double {
        const capture_t &cap = caps[k.cap];
        double v = pp.damage_w * cap.damage[(size_t)k.frame] + pp.noise_w * noise[k.cap][(size_t)k.frame];
        if (k.matched)
            v += pp.score_w * std::max(0.0,1.0 - k.score);
        else
            v += pp.unmatched_cost * (k.score > 0 ? 0.25 : 1.0);
        if (is_repeat(cap,(size_t)k.frame))
            v += pp.repeat_cost;
        if (k.margin < 0)
            v += pp.misalign_w * -k.margin;
        return v;
    };

    // the least cost to each candidate of each slot, and the candidate of the slot before it that it is from
    const size_t n = tl.size();
    std::vector<std::vector<int> > from(n);
    std::vector<double> prev,cur;
    for (size_t s=0;s < n;s++) {
        const std::vector<cand_t> &c = tl[s].c;
        cur.assign(c.size(),0.0);
        from[s].assign(c.size(),-1);
        double best_prev = 1e300;
        int best_prev_i = -1;
        if (s > 0) {
            for (size_t j=0;j < prev.size();j++) {
                if (prev[j] < best_prev) {
                    best_prev = prev[j];
                    best_prev_i = (int)j;
                }
            }
        }
        for (size_t i=0;i < c.size();i++) {
            double v = 0;
            int f = -1;
            if (s > 0) {
                // a cut from the best of them, or going on from the frame before it in the same capture
                v = best_prev + pp.switch_cost;
                f = best_prev_i;
                const std::vector<cand_t> &p = tl[s - 1u].c;
                for (size_t j=0;j < p.size();j++) {
                    if (p[j].cap == c[i].cap && follows(caps[c[i].cap],p[j].frame,c[i].frame) && prev[j] < v) {
                        v = prev[j];
                        f = (int)j;
                    }
                }
            }
            cur[i] = v + cost(c[i]);
            from[s][i] = f;
        }
        prev.swap(cur);
    }

    choice.assign(n,-1);
    if (n == 0)
        return;
    int k = (int)(std::min_element(prev.begin(),prev.end()) - prev.begin());
    for (size_t s=n;s-- > 0;) {
        choice[s] = k;
        k = from[s][(size_t)k];
    }

    // a frame that is only a repeat of the frame before it, as a capture card puts in for one it dropped, in
    // every capture that has it, is cut out: it is not that frame
    if (!pp.keep_repeats) {
        for (size_t s=0;s < n;s++) {
            const cand_t &c = tl[s].c[(size_t)choice[s]];
            if (is_repeat(caps[c.cap],(size_t)c.frame))
                choice[s] = -1;
        }
    }

    if (pp.cut >= 0) {
        for (size_t s=0;s < n;s++) {
            if (choice[s] < 0)
                continue;
            const cand_t &c = tl[s].c[(size_t)choice[s]];
            const capture_t &cap = caps[c.cap];
            if (cap.cells != 0 && (double)cap.damage[(size_t)c.frame] > pp.cut * (double)cap.cells)
                choice[s] = -1;
        }
    }
}

// add a frame of a capture to the clips, going on with the last one if it follows it
static void add_frame(std::vector<clip_t> &out,unsigned int cap,int64_t frame,int64_t pos,const std::vector<capture_t> &caps) {
    if (!out.empty()) {
        clip_t &l = out.back();
        if (l.cap == cap && l.start + l.length == pos && follows(caps[cap],l.frame + l.length - 1,frame)) {
            l.length++;
            return;
        }
    }
    clip_t c;
    c.cap = cap;
    c.frame = frame;
    c.length = 1;
    c.start = pos;
    out.push_back(c);
}

void best_clips(std::vector<clip_t> &out,const std::vector<slot_t> &tl,const std::vector<int> &choice,const std::vector<capture_t> &caps) {
    int64_t pos = 0;
    out.clear();
    for (size_t s=0;s < tl.size();s++) {
        if (choice[s] < 0)
            continue;
        const cand_t &c = tl[s].c[(size_t)choice[s]];
        add_frame(out,c.cap,c.frame,pos++,caps);
    }
}

void capture_clips(std::vector<clip_t> &out,unsigned int cap,const std::vector<slot_t> &tl,const std::vector<int> &choice,const std::vector<capture_t> &caps) {
    int64_t pos = 0,last = -2;
    out.clear();
    for (size_t s=0;s < tl.size();s++) {
        if (choice[s] < 0)
            continue;
        // the frame of the capture that goes on from the one before, or else the best that matched
        int best = -1;
        for (size_t i=0;i < tl[s].c.size();i++) {
            const cand_t &c = tl[s].c[i];
            if (c.cap != cap)
                continue;
            if (last >= 0 && follows(caps[cap],last,c.frame)) {
                best = (int)i;
                break;
            }
            if (best < 0 || (c.matched && !tl[s].c[(size_t)best].matched) ||
                (c.matched == tl[s].c[(size_t)best].matched && c.score > tl[s].c[(size_t)best].score))
                best = (int)i;
        }
        if (best >= 0) {
            last = tl[s].c[(size_t)best].frame;
            add_frame(out,cap,last,pos,caps);
        }
        else {
            last = -2;
        }
        pos++;
    }
}
