#ifndef VHSDROPOUT_FILL_H
#define VHSDROPOUT_FILL_H

#include <vector>

#include "plane.h"
#include "motion.h"

// a picture before or after the one being filled in, to fill it in from
struct fill_source_t {
    const std::vector<plane16_t>* pl = NULL;
    const mask_t*               mask = NULL;        // its own dropouts, of its luma
    const mvfield_t*            mv = NULL;          // how the picture moved from the one being filled in to it
    int                         distance = 1;       // pictures away
};

struct fill_params_t {
    float                       cost_scale = 3.0f;  // a picture whose blocks match this much worse counts for 1/e as much
    int                         match_x = 24;       // pixels of luma each side, and lines above and below, of a dropout
    int                         match_y = 4;        // over which what is filled in is made as bright as the picture around it
    bool                        match = true;
};

struct fill_stats_t {
    size_t                      temporal = 0;       // samples of luma filled in from other pictures
    size_t                      spatial = 0;        // samples of luma filled in from the lines above and below
};

// the dropouts of a plane of a picture, from the dropouts of its luma
void plane_mask(mask_t &pm,const mask_t &mask,int lw,int lh,const plane16_t &pl);

// -mode 3: fill in the dropouts of cur from the pictures around it, moved to where it is, and made
// as bright as the picture around the dropouts, or where none of them have it, from the lines
// above and below. lw and lh are the size of the luma.
void fill_motion(std::vector<plane16_t> &out,const std::vector<plane16_t> &cur,const mask_t &mask,int lw,int lh,
    const std::vector<fill_source_t> &src,const fill_params_t &fp,fill_stats_t &fs);

// -mode 1: fill in the dropouts of cur with the same pixels of held, the last of each that was not a dropout,
// or if held is NULL, from the lines above and below
void fill_hold(std::vector<plane16_t> &out,const std::vector<plane16_t> &cur,const mask_t &mask,int lw,int lh,
    const std::vector<plane16_t> *held,fill_stats_t &fs);

#endif //VHSDROPOUT_FILL_H
