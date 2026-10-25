#ifndef MCM_TIMELINE_H
#define MCM_TIMELINE_H

#include <stdint.h>

#include <vector>

#include "capture.h"

enum match_by_t {
    MATCH_VIDEO=1,                                  // by the pictures
    MATCH_AUDIO=2,                                  // by the sound
    MATCH_BOTH=3                                    // by the pictures, and by the sound where the pictures do not match
};

// a frame of a capture that is a frame of the timeline
struct cand_t {
    unsigned int                cap = 0;            // which capture
    int64_t                     frame = 0;          // which frame of it
    float                       score = 0;          // how well it matched the timeline, 1 for the frames that made it;
                                                    // if not matched, 0.5 if it was in a run as long as the slots
                                                    // between the frames either side that matched, 0 if not
    float                       margin = 0;         // how much less like the slot either side it is than its own: less
                                                    // than 0 if it is more like one of them, as a frame out may be
    bool                        matched = false;    // false: not matched itself, but put there from the frames
                                                    // either side of it that were, as one that is too damaged
};

// a frame of the timeline, and the frames of the captures that are it
struct slot_t {
    std::vector<cand_t>         c;
    unsigned int                src = 0;            // the one of them that the timeline is matched to
};

struct timeline_params_t {
    match_by_t                  by = MATCH_VIDEO;
    double                      min_score = 0.9;    // the least correlation of the thumbnails of the same frame
    int                         window = 300;       // frames matched at once, at first
    int                         insert_min = 8;     // a run of a capture this many frames longer than what the timeline
                                                    // has between the frames either side of it is put in the timeline
    bool                        keep_blank = false; // keep the frames that are blank in every capture
    bool                        quiet = false;
};

// Line up the captures on one timeline of frames: the frames of the first capture, ref, and then the frames
// of each of the others matched to it, and those that are not in it put in it where they go. A capture that
// does not match at all is left out, and said so. False if no capture matches another.
bool build_timeline(std::vector<slot_t> &tl,const std::vector<capture_t> &caps,unsigned int ref,const timeline_params_t &tp);

struct pick_params_t {
    double                      switch_cost = 3.0;  // of going to another capture, or jumping within one
    double                      damage_w = 0.2;     // of each cell of a frame that looks like a dropout
    double                      noise_w = 0.25;     // of each level of noise of a frame
    double                      score_w = 10.0;     // of each 0.1 less than a perfect match, times 10
    double                      unmatched_cost = 2.0;   // of a frame that was not matched itself
    double                      repeat_cost = 8.0;  // of a frame the same as the one before it in the capture
    double                      misalign_w = 50.0;  // of each 0.01 a frame is more like a slot either side of its own
    double                      cut = -1;           // a frame with more than this fraction of its cells dropouts, in the
                                                    // best capture there is of it, is cut out; < 0 never
    bool                        keep_repeats = false;   // keep a frame that a capture card repeated for one it dropped,
                                                    // where no capture has the frame it dropped
};

// The capture to take each frame of the timeline from: the one with the least damage, noise and doubt,
// going to another only where that is worth the cut. Each is the index of a candidate of the slot, or -1
// where the frame is cut out.
void pick_best(std::vector<int> &choice,const std::vector<slot_t> &tl,const std::vector<capture_t> &caps,const pick_params_t &pp);

// a run of frames of a capture, one after the other, on a track of the sequence
struct clip_t {
    unsigned int                cap = 0;
    int64_t                     frame = 0;          // the first frame of the capture
    int64_t                     length = 0;         // frames
    int64_t                     start = 0;          // where it is in the sequence, in frames
};

// the clips of the picks, on the sequence of the frames not cut out
void best_clips(std::vector<clip_t> &out,const std::vector<slot_t> &tl,const std::vector<int> &choice,const std::vector<capture_t> &caps);

// the clips of all of one capture that is in the sequence, lined up with it
void capture_clips(std::vector<clip_t> &out,unsigned int cap,const std::vector<slot_t> &tl,const std::vector<int> &choice,const std::vector<capture_t> &caps);

#endif //MCM_TIMELINE_H
