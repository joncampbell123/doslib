#include <string.h>
#include <stdio.h>
#include <math.h>

#include <algorithm>
#include <string>
#include <vector>

#define GL_GLEXT_PROTOTYPES 1
#include <GL/glcorearb.h>
#define EGL_EGLEXT_PROTOTYPES 1
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "gpu.h"

#include <stdlib.h>
#include <time.h>

// VMI_PROFILE in the environment says how long the GPU takes at each thing, at the end
static double prof_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}
static struct prof_t {
    double flow = 0,interp = 0,upload = 0;
    long nflow = 0,ninterp = 0,nupload = 0;
    bool on = getenv("VMI_PROFILE") != NULL;
    ~prof_t() {
        if (on)
            fprintf(stderr,"GPU: %ld uploads %.2f s, %ld flows %.2f s, %ld frames made %.2f s\n",nupload,upload,nflow,flow,ninterp,interp);
    }
} prof;

// the smallest image of a pyramid, across or down (as flow.cpp has it)
static const int PYR_MIN = 24;

// work groups of 8 by 8 invocations: blocks, or pixels
static const int GROUP = 8;

/////////////////////////////////////////////////////////////////////////////
// the shaders

// a level of a pyramid from the one before it, each pixel the mean of the four it is
static const char *SHADER_DOWN = R"GLSL(
#version 430
layout(local_size_x = 8,local_size_y = 8) in;
layout(binding = 0) uniform sampler2D src;
layout(r32f,binding = 0) writeonly uniform image2D dst;
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(p,imageSize(dst)))) return;
    ivec2 s = p * 2;
    float v = texelFetch(src,s,0).r + texelFetch(src,s + ivec2(1,0),0).r + texelFetch(src,s + ivec2(0,1),0).r + texelFetch(src,s + ivec2(1,1),0).r;
    imageStore(dst,p,vec4(v * 0.25));
}
)GLSL";

// How badly a block of a matches b moved by v, the mean of each taken off (as block_cost in flow.cpp), worked
// out by the 64 invocations of a work group together, each summing some of the pixels, the sums then added
// up in shared memory. Every invocation gets the same answer, and so goes on the same way.
#define GLSL_BLOCK_COST R"GLSL(
uniform int blk;
shared vec4 red[64];
float sample_b(vec2 p) {
    return texture(ib,(p + 0.5) / vec2(textureSize(ib,0))).r;
}
vec4 reduce(vec4 v) {
    uint i = gl_LocalInvocationIndex;
    red[i] = v;
    barrier();
    for (uint k=32u;k > 0u;k >>= 1u) {
        if (i < k)
            red[i] += red[i + k];
        barrier();
    }
    vec4 r = red[0];
    barrier();
    return r;
}
// how much more contrasty b is than a (as contrast_gain in flow.cpp)
float contrast_gain(float ma,float mb,float qa,float qb) {
    float sa = sqrt(max(0.0,qa - (ma * ma))),sb = sqrt(max(0.0,qb - (mb * mb)));
    if (sa < 2.0 || sb < 2.0)
        return 1.0;
    return clamp(sb / sa,0.5,2.0);
}
float block_cost(ivec2 bxy,vec2 v,out float dc) {
    ivec2 asz = textureSize(ia,0),bsz = textureSize(ib,0);
    int m = max(1,blk / 4);
    ivec2 lo = max(ivec2(0),(bxy * blk) - m),hi = min(asz,((bxy + 1) * blk) + m);
    int ww = hi.x - lo.x;
    int np = ww * (hi.y - lo.y);
    bool whole = v == floor(v);
    ivec2 iv = ivec2(v);
    float n = float(max(1,np));
    vec4 sum = vec4(0.0);
    for (int i=int(gl_LocalInvocationIndex);i < np;i += 64) {
        ivec2 p = lo + ivec2(i % ww,i / ww);
        float a = texelFetch(ia,p,0).r;
        float b = whole ? texelFetch(ib,clamp(p + iv,ivec2(0),bsz - 1),0).r : sample_b(vec2(p) + v);
        sum += vec4(a,b,a * a,b * b);
    }
    sum = reduce(sum) / n;
    dc = sum.y - sum.x;
    float g = contrast_gain(sum.x,sum.y,sum.z,sum.w);
    float s = 0.0;
    for (int i=int(gl_LocalInvocationIndex);i < np;i += 64) {
        ivec2 p = lo + ivec2(i % ww,i / ww);
        float a = texelFetch(ia,p,0).r;
        float b = whole ? texelFetch(ib,clamp(p + iv,ivec2(0),bsz - 1),0).r : sample_b(vec2(p) + v);
        s += abs(((a - sum.x) * g) + sum.y - b);
    }
    return reduce(vec4(s,0.0,0.0,0.0)).x / n;
}
)GLSL"

// the vector of each block at a level of the pyramid (as the loop of estimate_flow in flow.cpp), a work
// group to each block
static const char *SHADER_MATCH = "#version 430\n"
"layout(local_size_x = 64) in;\n"
"layout(binding = 0) uniform sampler2D ia;\n"
"layout(binding = 1) uniform sampler2D ib;\n"
"layout(binding = 2) uniform sampler2D coarse;\n"
"layout(binding = 3) uniform sampler2D pred;\n"
"layout(rgba32f,binding = 0) writeonly uniform image2D outf;\n"
GLSL_BLOCK_COST R"GLSL(
uniform int full;               // search all over, as far as this, at the coarsest level
uniform int coarse_block;
uniform int has_pred;
uniform int pred_block;
uniform float pred_k;           // the prediction is the field pred times this
uniform float scale;            // of this level: 2 to the level
uniform float smooth_w,predict_w,still_w;
uniform int subpel;

vec2 field_at(sampler2D f,int fb,vec2 p) {
    return texture(f,(p + 0.5) / (float(fb) * vec2(textureSize(f,0)))).xy;
}
float median9(float v[9],int n) {
    for (int i=1;i < n;i++) {
        float x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }
    return v[n / 2];
}

ivec2 bxy;
float mx,my,px,py;
float cost(vec2 v) {
    float dc;
    float c = block_cost(bxy,v,dc);
    c += still_w * scale * (abs(v.x) + abs(v.y));
    if (full == 0)
        c += smooth_w * (abs(v.x - mx) + abs(v.y - my));
    if (has_pred != 0)
        c += predict_w * (abs(v.x - px) + abs(v.y - py));
    return c;
}

void main() {
    bxy = ivec2(gl_WorkGroupID.xy);
    vec2 cxy = (vec2(bxy) + 0.5) * float(blk);
    vec2 best = vec2(0.0);
    float bc = 1e30;
    mx = 0.0;
    my = 0.0;
    px = 0.0;
    py = 0.0;
    if (has_pred != 0) {
        vec2 p = field_at(pred,pred_block,cxy * scale) * pred_k / scale;
        px = p.x;
        py = p.y;
    }

    if (full > 0) {
        for (int vy=-full;vy <= full;vy++) {
            for (int vx=-full;vx <= full;vx++) {
                float c = cost(vec2(vx,vy));
                if (c < bc) {
                    bc = c;
                    best = vec2(vx,vy);
                }
            }
        }
    }
    else {
        float xs[9],ys[9];
        vec2 cand[9];
        int n = 0;
        for (int dy=-1;dy <= 1;dy++) {
            for (int dx=-1;dx <= 1;dx++) {
                vec2 v = field_at(coarse,coarse_block,(cxy + vec2(dx * blk,dy * blk)) * 0.5) * 2.0;
                xs[n] = v.x;
                ys[n] = v.y;
                cand[n] = floor(v + 0.5);
                n++;
            }
        }
        mx = median9(xs,n);
        my = median9(ys,n);
        for (int i=0;i < 9;i++) {
            float c = cost(cand[i]);
            if (c < bc) {
                bc = c;
                best = cand[i];
            }
        }
        float c = cost(vec2(0.0));
        if (c < bc) {
            bc = c;
            best = vec2(0.0);
        }
    }
    if (has_pred != 0) {
        vec2 v = floor(vec2(px,py) + 0.5);
        float c = cost(v);
        if (c < bc) {
            bc = c;
            best = v;
        }
    }

    // a pixel at a time around the best, and at the full size, a half and a quarter of one
    const ivec2 off[8] = ivec2[8](ivec2(-1,0),ivec2(1,0),ivec2(0,-1),ivec2(0,1),ivec2(-1,-1),ivec2(1,-1),ivec2(-1,1),ivec2(1,1));
    for (int it=0;it < 4;it++) {
        bool moved = false;
        for (int d=0;d < 8;d++) {
            vec2 v = best + vec2(off[d]);
            float c = cost(v);
            if (c < bc) {
                bc = c;
                best = v;
                moved = true;
            }
        }
        if (!moved) break;
    }
    if (subpel != 0) {
        for (float step=0.5;step >= 0.25;step *= 0.5) {
            vec2 nb = best;
            for (int dy=-1;dy <= 1;dy++) {
                for (int dx=-1;dx <= 1;dx++) {
                    if (dx == 0 && dy == 0) continue;
                    vec2 v = best + (step * vec2(dx,dy));
                    float c = cost(v);
                    if (c < bc) {
                        bc = c;
                        nb = v;
                    }
                }
            }
            best = nb;
        }
    }
    if (gl_LocalInvocationIndex == 0u)
        imageStore(outf,bxy,vec4(best,0.0,0.0));
}
)GLSL";

// each vector the median of it and the vectors around it (as median_filter in flow.cpp)
static const char *SHADER_MEDIAN = R"GLSL(
#version 430
layout(local_size_x = 8,local_size_y = 8) in;
layout(binding = 0) uniform sampler2D src;
layout(rgba32f,binding = 0) writeonly uniform image2D dst;
float median9(float v[9],int n) {
    for (int i=1;i < n;i++) {
        float x = v[i];
        int j = i - 1;
        while (j >= 0 && v[j] > x) {
            v[j + 1] = v[j];
            j--;
        }
        v[j + 1] = x;
    }
    return v[n / 2];
}
void main() {
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    ivec2 sz = textureSize(src,0);
    if (any(greaterThanEqual(p,sz))) return;
    float xs[9],ys[9];
    int n = 0;
    for (int dy=-1;dy <= 1;dy++) {
        for (int dx=-1;dx <= 1;dx++) {
            ivec2 q = p + ivec2(dx,dy);
            if (any(lessThan(q,ivec2(0))) || any(greaterThanEqual(q,sz))) continue;
            vec4 v = texelFetch(src,q,0);
            xs[n] = v.x;
            ys[n] = v.y;
            n++;
        }
    }
    imageStore(dst,p,vec4(median9(xs,n),median9(ys,n),0.0,0.0));
}
)GLSL";

// how well each block matches where its vector puts it, and how much brighter it is there, a work group
// to each block
static const char *SHADER_COST = "#version 430\n"
"layout(local_size_x = 64) in;\n"
"layout(binding = 0) uniform sampler2D ia;\n"
"layout(binding = 1) uniform sampler2D ib;\n"
"layout(binding = 2) uniform sampler2D vec;\n"
"layout(rgba32f,binding = 0) writeonly uniform image2D outf;\n"
GLSL_BLOCK_COST R"GLSL(
void main() {
    ivec2 p = ivec2(gl_WorkGroupID.xy);
    vec2 v = texelFetch(vec,p,0).xy;
    float dc;
    float c = block_cost(p,v,dc);
    if (gl_LocalInvocationIndex == 0u)
        imageStore(outf,p,vec4(v,c,dc));
}
)GLSL";

// the new frame (as interpolate() in interp.cpp)
static const char *SHADER_INTERP = R"GLSL(
#version 430
layout(local_size_x = 8,local_size_y = 8) in;
layout(binding = 0) uniform sampler2D pa;       // the planes of a and b, upscaled
layout(binding = 1) uniform sampler2D pb;
layout(binding = 2) uniform sampler2D la;       // their luma
layout(binding = 3) uniform sampler2D lb;
layout(binding = 4) uniform sampler2D fab;      // the motion from a to b, b to a, a to the frame before, b to the frame after
layout(binding = 5) uniform sampler2D fba;
layout(binding = 6) uniform sampler2D fap;
layout(binding = 7) uniform sampler2D fbn;
layout(rgba8,binding = 0) writeonly uniform image2D dst;
uniform int blk;
uniform int has_prev,has_next;
uniform float ka,kb;
uniform float t;
uniform float s;

const float MATCH_SIGMA = 10.0;
const float CONSISTENT_PIXELS = 1.0;
const float CONSISTENT_FRACTION = 0.15;
const float ONE_SIDED = 0.15;

vec4 field(sampler2D f,vec2 p) {
    return texture(f,(p + 0.5) / (float(blk) * vec2(textureSize(f,0))));
}
float luma(sampler2D l,vec2 p) {
    return texture(l,(p + 0.5) / vec2(textureSize(l,0))).r;
}
vec4 plane(sampler2D pl,vec2 p) {
    return texture(pl,(p + 0.5) / vec2(textureSize(pl,0))) * 255.0;
}
vec2 into_a(vec2 p,vec2 v) {
    return has_prev != 0 ? -field(fap,p).xy * ka : v;
}
vec2 from_b(vec2 p,vec2 v) {
    return has_next != 0 ? field(fbn,p).xy * kb : v;
}
vec2 keep(vec2 tg,vec2 v,float lim) {
    vec2 d = tg - v;
    float l = length(d);
    return l > lim ? v + (d * lim / l) : tg;
}
vec2 path(float u,vec2 v,vec2 inv,vec2 outv) {
    float lim = max(2.0,0.5 * length(v));
    vec2 ta = keep(0.5 * (v + inv),v,lim);
    vec2 tb = keep(0.5 * (v + outv),v,lim);
    float u2 = u * u,u3 = u2 * u;
    float h10 = u3 - (2.0 * u2) + u,h01 = (-2.0 * u3) + (3.0 * u2),h11 = u3 - u2;
    return (h10 * ta) + (h01 * v) + (h11 * tb);
}
float up(float v) {
    return ((v + 0.5) * s) - 0.5;
}

void main() {
    ivec2 P = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(P,imageSize(dst)))) return;
    vec2 xy = ((vec2(P) + 0.5) / s) - 0.5;

    // from a
    vec2 a = xy,v = vec2(0.0);
    for (int it=0;it < 4;it++) {
        v = field(fab,a).xy;
        vec2 o = path(t,v,into_a(a,v),from_b(a + v,v));
        a = xy - o;
    }
    v = field(fab,a).xy;

    // from b, going back
    vec2 b = xy,w = vec2(0.0);
    for (int it=0;it < 4;it++) {
        w = field(fba,b).xy;
        vec2 o = path(1.0 - t,w,-from_b(b,-w),-into_a(b + w,-w));
        b = xy - o;
    }
    w = field(fba,b).xy;

    vec2 r = field(fba,a + v).xy;
    float cf = abs(v.x + r.x) + abs(v.y + r.y);
    bool both_f = cf <= CONSISTENT_PIXELS + (CONSISTENT_FRACTION * (abs(v.x) + abs(v.y)));
    r = field(fab,b + w).xy;
    float cb = abs(w.x + r.x) + abs(w.y + r.y);
    bool both_b = cb <= CONSISTENT_PIXELS + (CONSISTENT_FRACTION * (abs(w.x) + abs(w.y)));

    float wf,wb;
    if (both_f)
        wf = exp(-abs(luma(la,a) + field(fab,a).w - luma(lb,a + v)) / MATCH_SIGMA);
    else
        wf = ONE_SIDED * (1.0 - t);
    if (both_b)
        wb = exp(-abs(luma(lb,b) + field(fba,b).w - luma(la,b + w)) / MATCH_SIGMA);
    else
        wb = ONE_SIDED * t;
    float sum = wf + wb;
    if (sum < 1e-6) {
        wf = 1.0 - t;
        wb = t;
    }
    else {
        wf /= sum;
        wb /= sum;
    }

    vec2 fa = vec2(up(a.x),up(a.y)),fb = vec2(up(a.x + v.x),up(a.y + v.y));
    vec2 ba = vec2(up(b.x + w.x),up(b.y + w.y)),bb = vec2(up(b.x),up(b.y));
    float fwa = both_f ? 1.0 - t : 1.0,fwb = both_f ? t : 0.0;
    float bwa = both_b ? 1.0 - t : 0.0,bwb = both_b ? t : 1.0;
    vec4 vf = (fwa * plane(pa,fa)) + (fwb * plane(pb,fb));
    vec4 vb = (bwb * plane(pb,bb)) + (bwa * plane(pa,ba));
    imageStore(dst,P,((wf * vf) + (wb * vb)) / 255.0);
}
)GLSL";

/////////////////////////////////////////////////////////////////////////////

gpu_t::~gpu_t() {
    if (ctx != NULL) {
        glDeleteProgram(p_down);
        glDeleteProgram(p_match);
        glDeleteProgram(p_median);
        glDeleteProgram(p_cost);
        glDeleteProgram(p_interp);
        glDeleteTextures(1,&dummy);
        eglMakeCurrent((EGLDisplay)dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        eglDestroyContext((EGLDisplay)dpy,(EGLContext)ctx);
    }
    if (dpy != NULL)
        eglTerminate((EGLDisplay)dpy);
}

bool gpu_t::compile(unsigned int &prog,const char *what,const char *src) {
    GLuint sh = glCreateShader(GL_COMPUTE_SHADER);
    GLint ok = 0;
    glShaderSource(sh,1,&src,NULL);
    glCompileShader(sh);
    glGetShaderiv(sh,GL_COMPILE_STATUS,&ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(sh,sizeof(log),NULL,log);
        fprintf(stderr,"The %s shader does not compile:\n%s\n",what,log);
        glDeleteShader(sh);
        return false;
    }
    prog = glCreateProgram();
    glAttachShader(prog,sh);
    glLinkProgram(prog);
    glDeleteShader(sh);
    glGetProgramiv(prog,GL_LINK_STATUS,&ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(prog,sizeof(log),NULL,log);
        fprintf(stderr,"The %s shader does not link:\n%s\n",what,log);
        return false;
    }
    return true;
}

bool gpu_t::init(int device,bool quiet) {
    // each GPU the EGL device platform says there is, without a display, or else the surfaceless platform
    PFNEGLQUERYDEVICESEXTPROC query_devices = (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
    PFNEGLGETPLATFORMDISPLAYEXTPROC platform_display = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    std::vector<EGLDisplay> displays;
    if (query_devices != NULL && platform_display != NULL) {
        EGLDeviceEXT devs[16];
        EGLint n = 0;
        if (query_devices(16,devs,&n) && n > 0) {
            for (EGLint i=0;i < n;i++) {
                EGLDisplay d = platform_display(EGL_PLATFORM_DEVICE_EXT,devs[i],NULL);
                if (d != EGL_NO_DISPLAY)
                    displays.push_back(d);
            }
        }
    }
    if (displays.empty() && platform_display != NULL) {
        EGLDisplay d = platform_display(EGL_PLATFORM_SURFACELESS_MESA,EGL_DEFAULT_DISPLAY,NULL);
        if (d != EGL_NO_DISPLAY)
            displays.push_back(d);
    }

    int usable = 0;
    std::vector<std::string> tried;
    for (EGLDisplay d : displays) {
        EGLint major,minor;
        if (!eglInitialize(d,&major,&minor))
            continue;
        if (!eglBindAPI(EGL_OPENGL_API)) {
            eglTerminate(d);
            continue;
        }
        const EGLint cattr[] = { EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,EGL_NONE };
        EGLConfig cfg = NULL;
        EGLint ncfg = 0;
        const bool have_cfg = eglChooseConfig(d,cattr,&cfg,1,&ncfg) && ncfg > 0;
        const EGLint attr[] = { EGL_CONTEXT_MAJOR_VERSION,4,EGL_CONTEXT_MINOR_VERSION,3,
                                EGL_CONTEXT_OPENGL_PROFILE_MASK,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,EGL_NONE };
        EGLContext c = eglCreateContext(d,have_cfg ? cfg : EGL_NO_CONFIG_KHR,EGL_NO_CONTEXT,attr);
        if (c == EGL_NO_CONTEXT || !eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,c)) {
            if (c != EGL_NO_CONTEXT)
                eglDestroyContext(d,c);
            eglTerminate(d);
            continue;
        }
        const char *r = (const char*)glGetString(GL_RENDERER);
        const std::string rn = r != NULL ? r : "?";
        const bool software = rn.find("llvmpipe") != std::string::npos || rn.find("softpipe") != std::string::npos ||
                              rn.find("Software") != std::string::npos;
        // the same GPU can be there more than once, by more than one platform
        const bool again = std::find(tried.begin(),tried.end(),rn) != tried.end();
        tried.push_back(rn);
        const bool take = !again && (device >= 0 ? usable == device : !software);
        if (!again)
            usable++;
        if (take) {
            dpy = d;
            ctx = c;
            name = rn;
            break;
        }
        eglMakeCurrent(d,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        eglDestroyContext(d,c);
        eglTerminate(d);
    }
    if (ctx == NULL) {
        if (tried.empty())
            fprintf(stderr,"No GPU can do OpenGL 4.3, which this needs, without a display\n");
        else if (device >= 0)
            fprintf(stderr,"There is no GPU %d: there are %d (-gpu 0 to %d)\n",device,usable,usable - 1);
        else
            fprintf(stderr,"The only OpenGL 4.3 there is is a software renderer (%s), which is slower than the CPU version (-gpu 0 takes it anyway)\n",tried[0].c_str());
        return false;
    }
    if (!quiet)
        fprintf(stderr,"GPU: %s, %s\n",name.c_str(),(const char*)glGetString(GL_VERSION));

    if (!compile(p_down,"pyramid",SHADER_DOWN) || !compile(p_match,"block matching",SHADER_MATCH) ||
        !compile(p_median,"median",SHADER_MEDIAN) || !compile(p_cost,"block cost",SHADER_COST) ||
        !compile(p_interp,"interpolation",SHADER_INTERP))
        return false;

    dummy = texture(1,1,GL_RGBA32F);
    const float zero[4] = { 0,0,0,0 };
    glTextureSubImage2D(dummy,0,0,0,1,1,GL_RGBA,GL_FLOAT,zero);
    return true;
}

// a texture of that size and format, sampled between texels, the edges going on outside it
gpu_tex_t gpu_t::texture(int w,int h,unsigned int format) {
    GLuint t = 0;
    glCreateTextures(GL_TEXTURE_2D,1,&t);
    glTextureStorage2D(t,1,format,w,h);
    glTextureParameteri(t,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTextureParameteri(t,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTextureParameteri(t,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTextureParameteri(t,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    return t;
}

// w by h invocations, in work groups of GROUP by GROUP; or with groups, w by h work groups
static void dispatch(int w,int h,bool groups=false) {
    if (groups)
        glDispatchCompute((GLuint)w,(GLuint)h,1);
    else
        glDispatchCompute((GLuint)((w + GROUP - 1) / GROUP),(GLuint)((h + GROUP - 1) / GROUP),1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT);
}

static GLint loc(GLuint prog,const char *n) {
    return glGetUniformLocation(prog,n);
}

bool gpu_t::upload(gpu_frame_t &f,const fimage_t &luma,const uint8_t *planes,int nplanes,int pw,int ph) {
    const double t0 = prof.on ? prof_now() : 0;
    free_frame(f);

    // the luma, and the levels of the pyramid from it, on the GPU
    int w = luma.w,h = luma.h;
    f.luma.push_back(texture(w,h,GL_R32F));
    f.lw.push_back(w);
    f.lh.push_back(h);
    glTextureSubImage2D(f.luma[0],0,0,0,w,h,GL_RED,GL_FLOAT,&luma.v[0]);
    glUseProgram(p_down);
    while (std::min(w,h) / 2 >= PYR_MIN) {
        w /= 2;
        h /= 2;
        const gpu_tex_t t = texture(w,h,GL_R32F);
        glBindTextureUnit(0,f.luma.back());
        glBindImageTexture(0,t,0,GL_FALSE,0,GL_WRITE_ONLY,GL_R32F);
        dispatch(w,h);
        f.luma.push_back(t);
        f.lw.push_back(w);
        f.lh.push_back(h);
    }

    // the planes, in the channels of one texture
    if (planes != NULL) {
        const size_t n = (size_t)pw * (size_t)ph;
        pack.assign(n * 4u,0);
        for (int p=0;p < nplanes && p < 4;p++) {
            const uint8_t *s = planes + ((size_t)p * n);
            for (size_t i=0;i < n;i++)
                pack[(i * 4u) + (size_t)p] = s[i];
        }
        f.planes = texture(pw,ph,GL_RGBA8);
        f.pw = pw;
        f.ph = ph;
        glTextureSubImage2D(f.planes,0,0,0,pw,ph,GL_RGBA,GL_UNSIGNED_BYTE,&pack[0]);
    }
    if (prof.on) {
        glFinish();
        prof.upload += prof_now() - t0;
        prof.nupload++;
    }
    return glGetError() == GL_NO_ERROR;
}

void gpu_t::free_frame(gpu_frame_t &f) {
    if (!f.luma.empty())
        glDeleteTextures((GLsizei)f.luma.size(),&f.luma[0]);
    if (f.planes != 0)
        glDeleteTextures(1,&f.planes);
    f = gpu_frame_t();
}

void gpu_t::free_field(gpu_field_t &f) {
    if (f.tex != 0)
        glDeleteTextures(1,&f.tex);
    f = gpu_field_t();
}

bool gpu_t::flow(gpu_field_t &out,const gpu_frame_t &a,const gpu_frame_t &b,const flow_params_t &fp,const gpu_field_t *pred,float pred_k) {
    const int levels = (int)std::min(a.luma.size(),b.luma.size());
    const int blk = fp.block;
    const bool has_pred = pred != NULL && !pred->empty();
    gpu_field_t coarse;
    const double t0 = prof.on ? prof_now() : 0;

    free_field(out);
    for (int L=levels - 1;L >= 0;L--) {
        const int w = a.lw[(size_t)L],h = a.lh[(size_t)L];
        const int bw = (w + blk - 1) / blk,bh = (h + blk - 1) / blk;
        const float scale = (float)(1 << L);
        const int full = L == levels - 1 ? (int)ceilf((float)fp.range / scale) + 1 : 0;
        const gpu_tex_t raw = texture(bw,bh,GL_RGBA32F);

        glUseProgram(p_match);
        glBindTextureUnit(0,a.luma[(size_t)L]);
        glBindTextureUnit(1,b.luma[(size_t)L]);
        glBindTextureUnit(2,coarse.empty() ? dummy : coarse.tex);
        glBindTextureUnit(3,has_pred ? pred->tex : dummy);
        glBindImageTexture(0,raw,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);
        glUniform1i(loc(p_match,"blk"),blk);
        glUniform1i(loc(p_match,"full"),full);
        glUniform1i(loc(p_match,"coarse_block"),blk);
        glUniform1i(loc(p_match,"has_pred"),has_pred ? 1 : 0);
        glUniform1i(loc(p_match,"pred_block"),has_pred ? pred->block : blk);
        glUniform1f(loc(p_match,"pred_k"),pred_k);
        glUniform1f(loc(p_match,"scale"),scale);
        glUniform1f(loc(p_match,"smooth_w"),fp.smooth);
        glUniform1f(loc(p_match,"predict_w"),fp.predict);
        glUniform1f(loc(p_match,"still_w"),fp.still);
        glUniform1i(loc(p_match,"subpel"),(L == 0 && fp.subpel) ? 1 : 0);
        dispatch(bw,bh,true);

        gpu_field_t f;
        f.tex = texture(bw,bh,GL_RGBA32F);
        f.bw = bw;
        f.bh = bh;
        f.block = blk;
        glUseProgram(p_median);
        glBindTextureUnit(0,raw);
        glBindImageTexture(0,f.tex,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);
        dispatch(bw,bh);
        glDeleteTextures(1,&raw);

        free_field(coarse);
        coarse = f;
    }

    // how well each block matches where its vector puts it
    out.tex = texture(coarse.bw,coarse.bh,GL_RGBA32F);
    out.bw = coarse.bw;
    out.bh = coarse.bh;
    out.block = blk;
    glUseProgram(p_cost);
    glBindTextureUnit(0,a.luma[0]);
    glBindTextureUnit(1,b.luma[0]);
    glBindTextureUnit(2,coarse.tex);
    glBindImageTexture(0,out.tex,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA32F);
    glUniform1i(loc(p_cost,"blk"),blk);
    dispatch(out.bw,out.bh,true);
    free_field(coarse);
    if (prof.on) {
        glFinish();
        prof.flow += prof_now() - t0;
        prof.nflow++;
    }
    return glGetError() == GL_NO_ERROR;
}

bool gpu_t::download(field_t &out,const gpu_field_t &f) {
    std::vector<float> v((size_t)f.bw * (size_t)f.bh * 4u);
    out.alloc(f.bw * f.block,f.bh * f.block,f.block);
    glGetTextureImage(f.tex,0,GL_RGBA,GL_FLOAT,(GLsizei)(v.size() * sizeof(float)),&v[0]);
    for (size_t i=0;i < out.vx.size();i++) {
        out.vx[i] = v[(i * 4u) + 0u];
        out.vy[i] = v[(i * 4u) + 1u];
        out.cost[i] = v[(i * 4u) + 2u];
        out.dc[i] = v[(i * 4u) + 3u];
    }
    return glGetError() == GL_NO_ERROR;
}

bool gpu_t::interpolate(std::vector<uint8_t> &out,const gpu_between_t &bt,float t,int nplanes) {
    const gpu_frame_t &A = *bt.a,&B = *bt.b;
    const int W = A.pw,H = A.ph;
    const gpu_tex_t dst = texture(W,H,GL_RGBA8);
    const double t0 = prof.on ? prof_now() : 0;

    glUseProgram(p_interp);
    glBindTextureUnit(0,A.planes);
    glBindTextureUnit(1,B.planes);
    glBindTextureUnit(2,A.luma[0]);
    glBindTextureUnit(3,B.luma[0]);
    glBindTextureUnit(4,bt.ab->tex);
    glBindTextureUnit(5,bt.ba->tex);
    glBindTextureUnit(6,bt.a_prev != NULL ? bt.a_prev->tex : dummy);
    glBindTextureUnit(7,bt.b_next != NULL ? bt.b_next->tex : dummy);
    glBindImageTexture(0,dst,0,GL_FALSE,0,GL_WRITE_ONLY,GL_RGBA8);
    glUniform1i(loc(p_interp,"blk"),bt.ab->block);
    glUniform1i(loc(p_interp,"has_prev"),bt.a_prev != NULL ? 1 : 0);
    glUniform1i(loc(p_interp,"has_next"),bt.b_next != NULL ? 1 : 0);
    glUniform1f(loc(p_interp,"ka"),bt.a_prev != NULL && bt.dt_prev > 0 ? (float)(bt.dt / bt.dt_prev) : 0.0f);
    glUniform1f(loc(p_interp,"kb"),bt.b_next != NULL && bt.dt_next > 0 ? (float)(bt.dt / bt.dt_next) : 0.0f);
    glUniform1f(loc(p_interp,"t"),t);
    glUniform1f(loc(p_interp,"s"),(float)W / (float)A.lw[0]);
    dispatch(W,H);

    // back from the GPU, as planes
    const size_t n = (size_t)W * (size_t)H;
    pack.resize(n * 4u);
    glGetTextureImage(dst,0,GL_RGBA,GL_UNSIGNED_BYTE,(GLsizei)pack.size(),&pack[0]);
    glDeleteTextures(1,&dst);
    if (prof.on) {
        prof.interp += prof_now() - t0;
        prof.ninterp++;
    }
    out.resize(n * (size_t)nplanes);
    for (int p=0;p < nplanes;p++) {
        uint8_t *d = &out[(size_t)p * n];
        for (size_t i=0;i < n;i++)
            d[i] = pack[(i * 4u) + (size_t)p];
    }
    return glGetError() == GL_NO_ERROR;
}
