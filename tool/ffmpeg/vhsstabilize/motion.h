#ifndef VHSSTABILIZE_MOTION_H
#define VHSSTABILIZE_MOTION_H

#include <math.h>

#include <algorithm>
#include <vector>

#include "plane.h"

// the mean of each line of a picture, in vertical strips side by side, 0.0 (black) to 1.0.
// Line i of the profile is line first + (step * i) of the frame: a field has step 2.
struct profile_t {
    unsigned int                lines = 0;
    unsigned int                strips = 0;
    unsigned int                first = 0;
    unsigned int                step = 1;
    std::vector<float>          v;                  // v[(strip * lines) + line]

    const float *strip(unsigned int s) const {
        return &v[(size_t)s * lines];
    }
};

// make the profile of the lines first, first+step, first+(2*step), ... of a plane,
// leaving out 'left' and 'right' samples at each end of the lines
void make_profile(profile_t &p,const plane_t &pl,unsigned int first,unsigned int step,unsigned int strips,unsigned int left,unsigned int right);

// make the profile of a whole frame from the profiles of its two fields, where the picture
// in field 1 is d lines (of the frame) further down than the picture in field 0. Field 1 is
// moved to line up with field 0, so that the frame is a whole picture, as if it were not interlaced.
void merge_fields(profile_t &frame,const profile_t &f0,const profile_t &f1,double d,unsigned int height);

struct match_params_t {
    unsigned int                top = 0;            // lines of the frame at the top and bottom to leave out
    unsigned int                bottom = 0;
    unsigned int                range = 8;          // the most lines of the frame the picture can move
    double                      min_score = 0.6;    // the least score to trust a match
    double                      min_contrast = 0.002;   // the least standard deviation of a profile worth matching
};

struct match_t {
    bool                        ok = false;
    double                      shift = 0;          // lines of the frame the picture moved down (negative: up)
    double                      score = 0;          // -1 to 1, how well the profiles match
};

// how far the picture moved down from ref, a profile of every line of a frame, to cur, a profile
// of a frame or of one of its fields, to a fraction of a line. It is found by normalized
// cross-correlation of the profiles.
match_t match_profiles(const profile_t &ref,const profile_t &cur,const match_params_t &mp);

// how far the picture moved down from one field to another field of the same parity, in lines of the frame
match_t match_fields(const profile_t &ref,const profile_t &cur,const match_params_t &mp);

// follows the vertical position of the picture from one frame to the next, of the whole frame,
// or of each field.
//
// Each frame is matched to a key frame, as well as to the frame before it, so that small
// errors in the matches do not add up while the picture stays the same. The frame becomes
// the new key frame when the picture has changed enough that it no longer matches the key
// frame as well as it matches the frame before it.
//
// Fields are matched to the whole picture of the key frame or the frame before, made from
// both of its fields, so that a field that moved an odd number of lines of the frame, which
// puts its picture between the lines of the other fields of the same parity, is matched to
// the lines that are really there in the other field. A frame whose fields are an odd number
// of lines apart has no such lines, so as soon as there is a frame that has them, it becomes
// the key frame instead.
class tracker_t {
public:
    enum how_t {
        FIRST=0,                                    // the first frame, position 0
        KEY,                                        // matched to the key frame
        PREV,                                       // matched to the frame before
        LOST,                                       // could not be matched, kept the position of the frame before
        EARLIER                                     // matched to a frame before the frame before and the key frame
    };

    struct step_t {
        double                  pos = 0;            // lines of the frame the picture is down from the first frame
        double                  score = 0;
        how_t                   how = FIRST;
        unsigned int            branch = 0;         // which run of matches pos is from: the run starts again, and where
                                                    // the picture is from one run to another is not known, as at a cut
    };

    match_params_t              params;
    double                      key_score = 0.9;    // a new key frame when the key frame matches less than this
    double                      key_agree = 1.0;    // lines: the frame before is used instead of the key frame
                                                    // when they put the picture further apart than this
    unsigned int                height = 0;         // lines in the frame
    unsigned int                fields = 1;         // 2 to follow each field

    // add the next frame, the profiles of its fields (or the one profile of the frame),
    // and get where the picture is in each of them
    void add(const profile_t cur[],step_t st[]);
    // start again from this frame, with the picture where pos says it is in each field
    void restart_at(const profile_t cur[],const double pos[]);
    // the run of matches the tracker is in, from the frame it started again at
    unsigned int branch(void) const {
        return key.branch;
    }
private:
    struct ref_t {
        profile_t               frame;              // the whole frame, lined up with field 0
        double                  pos[2] = {0,0};
        bool                    found = true;       // false if a field of it could not be matched
        unsigned int            branch = 0;
    };

    static const size_t         EARLIER_FRAMES = 8;

    void make_ref(ref_t &r,const profile_t cur[],const double pos[]) const;

    ref_t                       key,prev;
    unsigned int                branches = 0;
    std::vector<ref_t>          earlier;            // the frames before prev, the last of them last
    bool                        key_is_prev = true;
    bool                        started = false;
};

// smooth a path with a gaussian of standard deviation radius/2, out to radius on each side,
// in which points far from the path count for less. If robust is false, they count the same.
std::vector<double> smooth_path(const std::vector<double> &path,double radius,bool robust=true);

// the median of each point of a path and the points up to radius on each side of it
std::vector<double> median_path(const std::vector<double> &path,unsigned int radius);

// how to tell the jitter of VHS from the movement of the camera
struct jump_params_t {
    double                      min_jump = 1.0;     // lines of the frame: a field this far or further from the path
                                                    // of the camera has jumped, and is all put back on it
    double                      stiffness = 2.0;    // how much the path of the camera is kept from bending, for 60 fields a second
    unsigned int                radius = 15;        // fields each side for where to start looking for the path, which
                                                    // leaves out jumps that come back sooner than this, for 60 fields a second
    unsigned int                rounds = 12;
    bool                        wide = true;        // first leave out only what is much further off, for a path that
                                                    // the median of the fields near it is a long way from, as a camera shaking

    // for this many fields (or frames) a second, so that the path bends as much in a second
    void set_rate(double rate) {
        const double r = rate / 59.94;
        stiffness = 2.0 * r * r * r * r;
        radius = (unsigned int)std::max(2.0,floor((15.0 * r) + 0.5));
    }
};

// The path of the camera, from the path of the picture, field after field in the order they are
// shown (or frame after frame): the path with the jumps of VHS taken out, and nothing else.
// known[i] is false where the picture could not be found, and branch[i] is the run of matches
// path[i] is from (the branch of the step), each of which has a path of its own.
// jumps, if not NULL, is set to how many fields had jumped.
std::vector<double> camera_path(const std::vector<double> &path,const std::vector<bool> &known,const std::vector<unsigned int> &branch,
    const jump_params_t &jp,size_t *jumps);

#endif //VHSSTABILIZE_MOTION_H
