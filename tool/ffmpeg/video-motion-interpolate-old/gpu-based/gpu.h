#ifndef VMI_GPU_H
#define VMI_GPU_H

#include <stdint.h>

#include <string>
#include <vector>

#include "../flow.h"

// The motion of the picture and the new frames, worked out on the GPU, with OpenGL 4.3 compute shaders,
// in a context of its own without a display (EGL). The frames, their pyramids and the motion between them
// stay on the GPU: only the frames read go up to it, and the frames made come back.

typedef unsigned int gpu_tex_t;                     // an OpenGL texture name

// a frame on the GPU: its luma at its own size, half, quarter, ... (as build_pyr makes them), and its color
// components, upscaled, in the channels of one texture
struct gpu_frame_t {
    std::vector<gpu_tex_t>      luma;
    std::vector<int>            lw,lh;              // the size of each level
    gpu_tex_t                   planes = 0;
    int                         pw = 0,ph = 0;      // the size of the planes
};

// how the picture moved from one frame to another, block by block, as field_t: x and y, how badly each
// block matches, and how much brighter it is where it moved to, in the four channels of a texture
struct gpu_field_t {
    gpu_tex_t                   tex = 0;
    int                         bw = 0,bh = 0,block = 8;

    bool empty(void) const {
        return tex == 0;
    }
};

// what the new frame is made from, as between_t
struct gpu_between_t {
    const gpu_frame_t*          a = NULL;
    const gpu_frame_t*          b = NULL;
    const gpu_field_t*          ab = NULL;
    const gpu_field_t*          ba = NULL;
    const gpu_field_t*          a_prev = NULL;
    const gpu_field_t*          b_next = NULL;
    double                      dt = 0,dt_prev = 0,dt_next = 0;
};

class gpu_t {
public:
    ~gpu_t();

    // the context, on GPU number device of those that can do it (-1 for the first that is not a software
    // renderer), and the shaders compiled. False, and why said, if it cannot be done.
    bool init(int device,bool quiet);
    const std::string &renderer(void) const {
        return name;
    }

    // a frame up to the GPU: its luma, and if planes is not NULL, nplanes components of pw by ph samples,
    // each plane pw bytes a line, one after the other
    bool upload(gpu_frame_t &f,const fimage_t &luma,const uint8_t *planes,int nplanes,int pw,int ph);
    void free_frame(gpu_frame_t &f);

    // how the picture moved from a to b, as estimate_flow; pred, if not NULL, times pred_k, is the
    // prediction of it, sampled at the middle of each block
    bool flow(gpu_field_t &out,const gpu_frame_t &a,const gpu_frame_t &b,const flow_params_t &fp,const gpu_field_t *pred,float pred_k);
    void free_field(gpu_field_t &f);

    // the vectors and costs of a field, back from the GPU
    bool download(field_t &out,const gpu_field_t &f);

    // the frame a fraction t of the way from a to b, as interpolate(), back from the GPU as nplanes planes
    // of the size of the frames' planes, one after the other
    bool interpolate(std::vector<uint8_t> &out,const gpu_between_t &bt,float t,int nplanes);
private:
    bool compile(unsigned int &prog,const char *what,const char *src);
    gpu_tex_t texture(int w,int h,unsigned int format);

    void*                       dpy = NULL;         // EGLDisplay
    void*                       ctx = NULL;         // EGLContext
    std::string                 name;
    unsigned int                p_down = 0,p_match = 0,p_median = 0,p_cost = 0,p_interp = 0;
    gpu_tex_t                   dummy = 0;          // bound where a field is not given
    std::vector<uint8_t>        pack;
};

#endif //VMI_GPU_H
