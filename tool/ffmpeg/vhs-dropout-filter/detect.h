#ifndef VHSDROPOUT_DETECT_H
#define VHSDROPOUT_DETECT_H

#include <vector>

#include "plane.h"

// how to find dropouts. Differences are in levels of luma, 0 to 255 whatever the depth of the video.
struct detect_params_t {
    float                       threshold = 10.0f;  // the least a dropout differs from the pictures before and after it
    float                       noise = 4.0f;       // or this many times as much as the picture typically differs
                                                    // from them, if more, for noisy or fast moving video
    float                       local = 3.0f;       // or this many times as much as the picture typically differs from
    int                         local_lines = 24;   // them in the lines this far above and below, if more, so that water,
                                                    // leaves and crowds, which are never the same twice, are not dropouts
    float                       prev = 0.5f;        // and this much of that where the pictures just before differed as much
    float                       repeat_detail = 2.0f;   // a line as much like the lines above it as repeat_ratio of how much
    float                       repeat_ratio = 0.35f;   // it changes across the line, where it changes across the line this much,
                                                    // is a line repeated, if it differs from the pictures around low as much
    float                       low = 0.5f;         // a dropout goes on into pixels that differ at least this much of that
    float                       strong = 3.0f;      // a dropout that differs this many times as much need not look like one
    int                         box = 9;            // pixels across that the differences are taken over
    int                         min_run = 16;       // pixels across: a dropout is at least this long on a line
    int                         gap = 6;            // pixels across: dropouts on a line this close together are one dropout
    int                         grow_x = 3;         // pixels each side, and lines above and below, that are put in with
    int                         grow_y = 1;         // a dropout, which the dropout smears into
    int                         margin[4] = {0,0,0,0};  // lines at the top and bottom, and pixels at the left and right,
                                                    // that are never dropouts
    float                       max_area = 0.6f;    // if more of the picture than this differs, it is not dropouts but
                                                    // something that is not in the pictures before and after, a flash
};

struct detect_stats_t {
    size_t                      pixels = 0;         // of luma that are dropouts
    unsigned int                lines = 0;          // that have dropouts on them
    float                       threshold = 0;      // the threshold used
    float                       noise = 0;          // how much the picture typically differs from the pictures around it
    bool                        too_much = false;   // max_area was more than reached, and nothing was taken to be a dropout
};

// find the dropouts in cur, from pred, the pictures before and after it moved to where it is.
// A dropout is on a line, or a few lines, of cur, and is in none of them, where they are not
// dropouts themselves, and looks like a dropout: the line repeated from the line above
// (by the dropout compensator of the VCR), or a line of noise, or a line that is not
// in keeping with the lines above and below it.
//
// E is set to how much each pixel differs from the pictures around it, and prevE is that of the pictures
// just before it, if there are any: where they differed as much, it is not a dropout, but something that is
// never the same twice, as sunlight on water.
void detect_dropouts(mask_t &mask,std::vector<float> &E,const image_t &cur,const std::vector<const image_t*> &pred,
    const std::vector<const std::vector<float>*> &prevE,const detect_params_t &dp,detect_stats_t &st);

// make each run of a line that is a dropout this many pixels longer each side and lines taller,
// the same as each that is grown into
void grow_mask(mask_t &mask,int w,int h,int gx,int gy);

#endif //VHSDROPOUT_DETECT_H
