#ifndef VHSDROPOUT_MATCH_H
#define VHSDROPOUT_MATCH_H

#include <stdint.h>

#include <vector>

// a thumbnail of the middle of each frame of a video, small enough to keep for hours of it
static const int THUMB_W = 32;
static const int THUMB_H = 20;
static const int THUMB_SIZE = THUMB_W * THUMB_H;

struct thumbs_t {
    std::vector<double>         time;               // seconds from the start of the file, of each frame
    std::vector<uint8_t>        v;                  // THUMB_SIZE gray samples of each frame

    size_t count(void) const {
        return time.size();
    }
    const uint8_t *thumb(size_t i) const {
        return &v[i * THUMB_SIZE];
    }
};

struct match_params_t {
    double                      min_score = 0.9;    // the least correlation of the thumbnails of two frames that are the same
    double                      min_gap = 0;        // seconds: frames this close in time are not matched, for a donor
                                                    // that is the same file as the input (another playback in it)
    int                         window = 300;       // frames of the input matched at once, at first
};

// the frame of the donor that is the same as each frame of the input, or -1, and how well it matches
struct frame_match_t {
    int64_t                     frame = -1;
    float                       score = 0;
};

// which frame of the donor each frame of the input is, from the thumbnails of each, by the pattern of how
// the picture changes from frame to frame (cuts, pans, things moving), which is the same in another copy
// whatever its brightness, colour and noise, and then by the thumbnails themselves, frame by frame.
// The donor may have only some parts of the input, in any order, as a copy edited from it does.
void match_frames(std::vector<frame_match_t> &out,const thumbs_t &in,const thumbs_t &donor,const match_params_t &mp);

#endif //VHSDROPOUT_MATCH_H
