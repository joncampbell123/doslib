/* vhsstabilize: take the up and down jitter out of video captured from a VHS tape.
 *
 * Reads a video file (a .mov) with FFmpeg, measures how far the picture moves up and down
 * from one frame to the next, then writes the video to another file with the fast up and
 * down movement taken out, leaving the slower movement of the camera. The video is decoded
 * and encoded again, but the audio is copied as it is.
 *
 * The input file is read twice: once to measure the whole video, and once to write it.
 * That way the smoothing can look ahead as far as it needs to. */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/avconfig.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "motion.h"
#include "vshift.h"

// vertical strips of the picture that are measured separately
static const unsigned int PROFILE_STRIPS = 8;

// frames to work out how the fields line up with each other, when the tracker starts again
static const size_t ANCHOR_FRAMES = 30;

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 log_path;
    std::string                 codec;              // video encoder name
    std::string                 codec_opts;         // key=value:key=value
    int64_t                     bit_rate = 0;
    unsigned int                range = 24;         // lines of the frame
    double                      max_shift = -1;     // lines of the frame, -1 for range
    double                      radius = 15;        // frames
    double                      min_score = 0.6;
    int                         margin[4] = {-1,-1,-1,-1}; // top, bottom, left, right; -1 for the default
    interp_t                    interp = INTERP_CUBIC;
    int                         fields = -1;        // -1 to decide from the file, 0 progressive, 1 interlaced
    bool                        measure_only = false;
    bool                        overwrite = false;
    bool                        quiet = false;
};

// what the first frame of the video is like
struct video_info_t {
    int                         width = 0;
    int                         height = 0;
    AVRational                  time_base = {1,1};  // of the video stream
    enum AVPixelFormat          dec_fmt = AV_PIX_FMT_NONE;     // as decoded
    enum AVPixelFormat          work_fmt = AV_PIX_FMT_NONE;    // as measured and moved
    AVRational                  sar = {0,1};
    enum AVColorRange           color_range = AVCOL_RANGE_UNSPECIFIED;
    enum AVColorPrimaries       color_primaries = AVCOL_PRI_UNSPECIFIED;
    enum AVColorTransferCharacteristic color_trc = AVCOL_TRC_UNSPECIFIED;
    enum AVColorSpace           colorspace = AVCOL_SPC_UNSPECIFIED;
    enum AVChromaLocation       chroma_location = AVCHROMA_LOC_UNSPECIFIED;
    enum AVFieldOrder           field_order = AV_FIELD_UNKNOWN;
    bool                        fields = true;      // measure and move each field on its own
};

// what was measured of one field (or of a whole frame, if progressive), and how to correct it,
// in lines of the frame
struct field_meas_t {
    tracker_t::step_t           step;
    double                      smooth = 0;         // where the picture should be
    double                      shift = 0;          // how far to move it down to get it there
};

struct frame_meas_t {
    int64_t                     pts = AV_NOPTS_VALUE;
    field_meas_t                f[2];
};

static void help(void) {
    fprintf(stderr,"vhsstabilize [options] <input.mov> <output.mov>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Takes the up and down jitter out of video captured from a VHS tape. The video is\n");
    fprintf(stderr,"encoded again, the audio is copied as it is. Lines are lines of the whole frame.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -r <lines>     The most the picture can move from one frame to the next (default 24)\n");
    fprintf(stderr,"  -w <frames>    Smoothing radius. Movement slower than this is kept, as movement of the\n");
    fprintf(stderr,"                 camera, and faster movement is taken out (default 15). 0 holds the picture\n");
    fprintf(stderr,"                 where it is most of the time, for the whole video.\n");
    fprintf(stderr,"  -M <lines>     The most the picture is moved to correct it (default: -r)\n");
    fprintf(stderr,"  -t <score>     The least match score, 0 to 1, to trust a measurement (default 0.6)\n");
    fprintf(stderr,"  -margin <top>,<bottom>,<left>,<right>\n");
    fprintf(stderr,"                 Lines and pixels at the edges to leave out of measuring, such as the head\n");
    fprintf(stderr,"                 switching noise at the bottom (default: 1/32 of the height at the top,\n");
    fprintf(stderr,"                 1/16 of the height at the bottom, 1/16 of the width at each side)\n");
    fprintf(stderr,"  -I             Interlaced: measure and move each field on its own\n");
    fprintf(stderr,"                 (default, unless the file says the video is progressive)\n");
    fprintf(stderr,"  -P             Progressive: measure and move whole frames\n");
    fprintf(stderr,"  -i <interp>    How to move the picture by a fraction of a line: cubic (default), linear,\n");
    fprintf(stderr,"                 or nearest (whole lines only, so nothing is blurred; with -I, whole lines\n");
    fprintf(stderr,"                 of the field, which are 2 lines of the frame)\n");
    fprintf(stderr,"  -c <encoder>   Video encoder, such as prores_ks, v210, ffv1 or libx264\n");
    fprintf(stderr,"                 (default: the input's codec, with prores_ks for ProRes)\n");
    fprintf(stderr,"  -b <rate>      Video bit rate, such as 50M (default: the input's, if the codec is the same)\n");
    fprintf(stderr,"  -x <k=v:k=v>   Video encoder options, such as -x profile=3 (ProRes HQ for prores_ks)\n");
    fprintf(stderr,"  -y             Write over the output file if it exists\n");
    fprintf(stderr,"  -n             Measure only, write no output file\n");
    fprintf(stderr,"  -log <file>    Write what was measured and corrected for each frame to a CSV file\n");
    fprintf(stderr,"  -q             Quiet\n");
    fprintf(stderr,"  -h             This help\n");
}

static std::string averr(int e) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(e,buf,sizeof(buf));
    return buf;
}

// true if the two names are, or would be, the same file
static bool same_file(const std::string &a,const std::string &b) {
    struct stat sa,sb;

    if (a == b)
        return true;
    if (stat(a.c_str(),&sa) != 0 || stat(b.c_str(),&sb) != 0)
        return false;

    return sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino;
}

static bool file_exists(const std::string &a) {
    struct stat s;
    return stat(a.c_str(),&s) == 0;
}

static double now_seconds(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC,&ts);
    return (double)ts.tv_sec + ((double)ts.tv_nsec / 1e9);
}

/////////////////////////////////////////////////////////////////////////////
// pixel formats

// true if each component is in its own plane, 8 bits in a byte or 9 to 16 bits in two bytes of host byte order.
// Those are the formats measured and moved: anything else is converted to one of them.
static bool pixfmt_workable(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);

    if (d == NULL || d->nb_components == 0)
        return false;
    if (d->flags & (AV_PIX_FMT_FLAG_HWACCEL | AV_PIX_FMT_FLAG_PAL | AV_PIX_FMT_FLAG_BITSTREAM | AV_PIX_FMT_FLAG_FLOAT))
        return false;

    for (unsigned int i=0;i < d->nb_components;i++) {
        const AVComponentDescriptor &c = d->comp[i];
        const int bytes = c.depth > 8 ? 2 : 1;

        for (unsigned int j=0;j < i;j++) {
            if (d->comp[j].plane == c.plane)
                return false;
        }
        if (c.depth > 16 || c.shift != 0 || c.offset != 0 || c.step != bytes)
            return false;
        if (bytes == 2 && (!!(d->flags & AV_PIX_FMT_FLAG_BE)) != (!!AV_HAVE_BIGENDIAN))
            return false;
    }

    return true;
}

// the old full range YUV formats
static bool pixfmt_is_yuvj(enum AVPixelFormat fmt) {
    const char *n = av_get_pix_fmt_name(fmt);
    return n != NULL && strncmp(n,"yuvj",4) == 0;
}

static bool pixfmt_has_alpha(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    return d != NULL && (d->flags & AV_PIX_FMT_FLAG_ALPHA);
}

// the workable format to convert a format that is not workable to, losing the least
static enum AVPixelFormat work_format_for(enum AVPixelFormat fmt) {
    static const enum AVPixelFormat candidates[] = {
        AV_PIX_FMT_YUV420P,     AV_PIX_FMT_YUV422P,     AV_PIX_FMT_YUV444P,     AV_PIX_FMT_YUV411P,     AV_PIX_FMT_YUV440P,
        AV_PIX_FMT_YUV420P10,   AV_PIX_FMT_YUV422P10,   AV_PIX_FMT_YUV444P10,
        AV_PIX_FMT_YUV420P12,   AV_PIX_FMT_YUV422P12,   AV_PIX_FMT_YUV444P12,
        AV_PIX_FMT_YUV420P16,   AV_PIX_FMT_YUV422P16,   AV_PIX_FMT_YUV444P16,
        AV_PIX_FMT_YUVA420P,    AV_PIX_FMT_YUVA422P,    AV_PIX_FMT_YUVA444P,
        AV_PIX_FMT_YUVA422P10,  AV_PIX_FMT_YUVA444P10,  AV_PIX_FMT_YUVA444P16,
        AV_PIX_FMT_GRAY8,       AV_PIX_FMT_GRAY10,      AV_PIX_FMT_GRAY12,      AV_PIX_FMT_GRAY16,
        AV_PIX_FMT_GBRP,        AV_PIX_FMT_GBRP10,      AV_PIX_FMT_GBRP12,      AV_PIX_FMT_GBRP16,
        AV_PIX_FMT_GBRAP,       AV_PIX_FMT_GBRAP16,
        AV_PIX_FMT_NONE
    };

    if (pixfmt_workable(fmt))
        return fmt;

    return avcodec_find_best_pix_fmt_of_list(candidates,fmt,pixfmt_has_alpha(fmt) ? 1 : 0,NULL);
}

static const enum AVPixelFormat *encoder_pix_fmts(const AVCodec *c) {
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61,13,100)
    const void *list = NULL;
    int count = 0;

    if (avcodec_get_supported_config(NULL,c,AV_CODEC_CONFIG_PIX_FORMAT,0,&list,&count) < 0)
        return NULL;

    return (const enum AVPixelFormat*)list;
#else
    return c->pix_fmts;
#endif
}

// one plane of a frame of a workable format, what component it is,
// and what fills the lines the picture moves away from: black, or opaque for alpha
struct frame_plane_t {
    plane_t                     pl;
    unsigned int                vsub = 0;           // log2 of the vertical subsampling
    unsigned int                fill = 0;
};

static std::vector<frame_plane_t> frame_planes(const AVFrame *f,bool full_range) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)f->format);
    std::vector<frame_plane_t> r;
    const bool rgb = !!(d->flags & AV_PIX_FMT_FLAG_RGB);
    const bool alpha = !!(d->flags & AV_PIX_FMT_FLAG_ALPHA);

    for (unsigned int i=0;i < d->nb_components;i++) {
        const AVComponentDescriptor &c = d->comp[i];
        const bool chroma = !rgb && (i == 1u || i == 2u) && d->nb_components >= 3u;
        frame_plane_t p;

        p.pl.data = f->data[c.plane];
        p.pl.stride = f->linesize[c.plane];
        p.pl.width = (unsigned int)(chroma ? AV_CEIL_RSHIFT(f->width,(int)d->log2_chroma_w) : f->width);
        p.pl.height = (unsigned int)(chroma ? AV_CEIL_RSHIFT(f->height,(int)d->log2_chroma_h) : f->height);
        p.pl.depth = (unsigned int)c.depth;
        p.pl.bytes = c.depth > 8 ? 2u : 1u;
        p.vsub = chroma ? d->log2_chroma_h : 0u;

        if (alpha && i == d->nb_components - 1u)
            p.fill = p.pl.maxval();
        else if (rgb)
            p.fill = 0;
        else if (chroma)
            p.fill = 1u << (p.pl.depth - 1u);
        else
            p.fill = full_range ? 0u : (16u << (p.pl.depth - 8u));

        r.push_back(p);
    }

    return r;
}

// the plane of a frame of a workable format to measure: luma, or green
static plane_t measure_plane(const AVFrame *f) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)f->format);
    const AVComponentDescriptor &c = d->comp[((d->flags & AV_PIX_FMT_FLAG_RGB) && d->nb_components >= 3u) ? 1 : 0];
    plane_t p;

    p.data = f->data[c.plane];
    p.stride = f->linesize[c.plane];
    p.width = (unsigned int)f->width;
    p.height = (unsigned int)f->height;
    p.depth = (unsigned int)c.depth;
    p.bytes = c.depth > 8 ? 2u : 1u;
    return p;
}

static bool frame_is_interlaced(const AVFrame *f) {
#ifdef AV_FRAME_FLAG_INTERLACED
    return !!(f->flags & AV_FRAME_FLAG_INTERLACED);
#else
    return !!f->interlaced_frame;
#endif
}

static bool frame_is_tff(const AVFrame *f) {
#ifdef AV_FRAME_FLAG_TOP_FIELD_FIRST
    return !!(f->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST);
#else
    return !!f->top_field_first;
#endif
}

static void frame_set_interlaced(AVFrame *f,bool tff) {
#ifdef AV_FRAME_FLAG_INTERLACED
    f->flags |= AV_FRAME_FLAG_INTERLACED;
    if (tff)
        f->flags |= AV_FRAME_FLAG_TOP_FIELD_FIRST;
    else
        f->flags &= ~AV_FRAME_FLAG_TOP_FIELD_FIRST;
#else
    f->interlaced_frame = 1;
    f->top_field_first = tff ? 1 : 0;
#endif
}

static bool field_order_interlaced(enum AVFieldOrder fo) {
    return fo == AV_FIELD_TT || fo == AV_FIELD_BB || fo == AV_FIELD_TB || fo == AV_FIELD_BT;
}

static const char *field_order_name(enum AVFieldOrder fo) {
    switch (fo) {
        case AV_FIELD_PROGRESSIVE:  return "progressive";
        case AV_FIELD_TT:           return "interlaced, top field first";
        case AV_FIELD_BB:           return "interlaced, bottom field first";
        case AV_FIELD_TB:           return "interlaced, top field coded first, bottom field shown first";
        case AV_FIELD_BT:           return "interlaced, bottom field coded first, top field shown first";
        default:                    break;
    }
    return "field order not given";
}

/////////////////////////////////////////////////////////////////////////////
// converting frames from one pixel format to another

class converter_t {
public:
    ~converter_t() {
        if (sws) sws_freeContext(sws);
    }

    // convert src into dst, which has its format and size set and its buffers allocated.
    // Levels stay as they are, full range or not, unless dst is a yuvj format, which is always full range.
    bool convert(AVFrame *dst,const AVFrame *src) {
        const enum AVPixelFormat sf = (enum AVPixelFormat)src->format;
        const enum AVPixelFormat df = (enum AVPixelFormat)dst->format;

        sws = sws_getCachedContext(sws,src->width,src->height,sf,dst->width,dst->height,df,
            SWS_BICUBIC | SWS_ACCURATE_RND | SWS_FULL_CHR_H_INT | SWS_FULL_CHR_H_INP,NULL,NULL,NULL);
        if (sws == NULL)
            return false;

        const int src_full = (src->color_range == AVCOL_RANGE_JPEG || pixfmt_is_yuvj(sf)) ? 1 : 0;
        const int dst_full = (src_full || pixfmt_is_yuvj(df)) ? 1 : 0;
        const int *coefs = sws_getCoefficients(src->colorspace == AVCOL_SPC_UNSPECIFIED ? SWS_CS_DEFAULT : (int)src->colorspace);
        sws_setColorspaceDetails(sws,coefs,src_full,coefs,dst_full,0,1 << 16,1 << 16);

        return sws_scale(sws,src->data,src->linesize,0,src->height,dst->data,dst->linesize) > 0;
    }
private:
    struct SwsContext*          sws = NULL;
};

// a frame of that format and size, with its buffers
static AVFrame *alloc_frame(enum AVPixelFormat fmt,int width,int height) {
    AVFrame *f = av_frame_alloc();

    if (f == NULL)
        return NULL;

    f->format = fmt;
    f->width = width;
    f->height = height;
    if (av_frame_get_buffer(f,0) < 0) {
        av_frame_free(&f);
        return NULL;
    }

    return f;
}

/////////////////////////////////////////////////////////////////////////////
// reading and decoding the input file

class input_t {
public:
    AVFormatContext*            fmt = NULL;
    AVCodecContext*             dec = NULL;
    int                         vindex = -1;

    ~input_t() {
        close();
    }

    void close(void) {
        avcodec_free_context(&dec);
        avformat_close_input(&fmt);
        vindex = -1;
    }

    AVStream *vstream(void) const {
        return fmt->streams[vindex];
    }

    // open the file and the decoder for its video. If video_only, the other streams are not read.
    bool open(const std::string &path,bool video_only) {
        int r;

        if ((r=avformat_open_input(&fmt,path.c_str(),NULL,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((r=avformat_find_stream_info(fmt,NULL)) < 0) {
            fprintf(stderr,"%s: cannot read the streams: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        if ((vindex=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,NULL,0)) < 0) {
            fprintf(stderr,"%s: no video stream\n",path.c_str());
            return false;
        }

        const AVStream *st = fmt->streams[vindex];
        const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
        if (codec == NULL) {
            fprintf(stderr,"%s: no decoder for %s video\n",path.c_str(),avcodec_get_name(st->codecpar->codec_id));
            return false;
        }

        if ((dec=avcodec_alloc_context3(codec)) == NULL)
            return false;
        if ((r=avcodec_parameters_to_context(dec,st->codecpar)) < 0) {
            fprintf(stderr,"%s: %s\n",path.c_str(),averr(r).c_str());
            return false;
        }
        dec->pkt_timebase = st->time_base;
        dec->thread_count = 0;
        if ((r=avcodec_open2(dec,codec,NULL)) < 0) {
            fprintf(stderr,"%s: cannot open the %s decoder: %s\n",path.c_str(),codec->name,averr(r).c_str());
            return false;
        }

        if (video_only) {
            for (unsigned int i=0;i < fmt->nb_streams;i++) {
                if ((int)i != vindex)
                    fmt->streams[i]->discard = AVDISCARD_ALL;
            }
        }

        return true;
    }

    // read the whole file: decode each frame of the video and give it to on_frame, and give
    // each packet of the other streams to on_packet. Either can stop it by returning false.
    bool run(const std::function<bool(AVFrame*)> &on_frame,const std::function<bool(AVPacket*)> &on_packet) {
        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        unsigned int errors = 0;
        bool ok = (pkt != NULL && frame != NULL);
        int r;

        // give on_frame each frame the decoder has ready
        auto drain = [&](void) -> bool {
            for (;;) {
                r = avcodec_receive_frame(dec,frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                    return true;
                if (r < 0) {
                    if (errors++ < 10u) fprintf(stderr,"\nVideo decoding error: %s\n",averr(r).c_str());
                    return true;
                }

                const bool keep = on_frame(frame);
                av_frame_unref(frame);
                if (!keep)
                    return false;
            }
        };

        while (ok) {
            if ((r=av_read_frame(fmt,pkt)) < 0) {
                if (r != AVERROR_EOF)
                    fprintf(stderr,"\nError reading the input, stopping there: %s\n",averr(r).c_str());
                break;
            }

            if (pkt->stream_index == vindex) {
                for (;;) {
                    r = avcodec_send_packet(dec,pkt);
                    if (r == AVERROR(EAGAIN)) {
                        if (!(ok=drain())) break;
                        continue;
                    }
                    if (r < 0 && errors++ < 10u)
                        fprintf(stderr,"\nVideo decoding error: %s\n",averr(r).c_str());
                    break;
                }
                if (ok)
                    ok = drain();
            }
            else if (on_packet) {
                ok = on_packet(pkt);
            }

            av_packet_unref(pkt);
        }

        // and the frames the decoder still has
        if (ok) {
            avcodec_send_packet(dec,NULL);
            ok = drain();
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
        return ok;
    }
};

class progress_t {
public:
    progress_t(const char *what,int64_t total,bool quiet) : what(what), total(total), quiet(quiet) { }

    void frame(unsigned long n) {
        const double t = now_seconds();
        if (!quiet && (t - last) >= 0.5) {
            show(n);
            last = t;
        }
    }

    void done(unsigned long n) {
        if (!quiet) {
            show(n);
            fprintf(stderr,"\n");
        }
    }
private:
    void show(unsigned long n) {
        if (total > 0)
            fprintf(stderr,"\r%s: frame %lu of %lld (%.1f%%)  ",what,n,(long long)total,(n * 100.0) / (double)total);
        else
            fprintf(stderr,"\r%s: frame %lu  ",what,n);
    }

    const char*                 what;
    int64_t                     total;
    bool                        quiet;
    double                      last = 0;
};

// how many frames the video stream has, or about how many, or 0 if not known
static int64_t frame_count(const input_t &in) {
    const AVStream *st = in.vstream();

    if (st->nb_frames > 0)
        return st->nb_frames;

    const AVRational fr = av_guess_frame_rate(in.fmt,(AVStream*)st,NULL);
    if (in.fmt->duration > 0 && fr.num > 0 && fr.den > 0)
        return av_rescale_q(in.fmt->duration,av_make_q(1,AV_TIME_BASE),av_inv_q(fr));

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
// first pass: measuring

static double median(std::vector<double> v) {
    if (v.empty())
        return 0;

    std::nth_element(v.begin(),v.begin() + (ptrdiff_t)(v.size() / 2u),v.end());
    return v[v.size() / 2u];
}

// How far the picture in field 1 of the first of these frames is below where it is in field 0,
// from how the fields of these frames are lined up most of the time, or false if it cannot tell.
//
// This matches each field only to other fields of the same parity, so it does not depend on
// knowing how the fields of any frame line up with each other, as matching to a whole frame does.
static bool field_offset(const std::vector<profile_t> &prof,const match_params_t &mp,enum AVFieldOrder fo,double &d) {
    const size_t n = prof.size() / 2u;
    std::vector<double> path[2];
    std::vector<bool> known[2];
    std::vector<double> misalign;

    // where the picture is in each field, from where it is in the first frame
    for (unsigned int p=0;p < 2u;p++) {
        path[p].assign(n,0.0);
        known[p].assign(n,false);
        known[p][0] = true;

        for (size_t i=1;i < n;i++) {
            match_t m = match_fields(prof[p],prof[(i * 2u) + p],mp);
            if (m.ok) {
                path[p][i] = m.shift;
                known[p][i] = true;
            }
            else if (known[p][i - 1u] && (m=match_fields(prof[((i - 1u) * 2u) + p],prof[(i * 2u) + p],mp)).ok) {
                path[p][i] = path[p][i - 1u] + m.shift;
                known[p][i] = true;
            }
        }
    }

    // field 1 is lined up with field 0 when it is where field 0 is at the same time,
    // halfway between where it is in this frame and the frame before or after
    for (size_t i=0;i < n;i++) {
        if (!known[0][i] || !known[1][i])
            continue;

        if (fo == AV_FIELD_TT || fo == AV_FIELD_TB) {
            if (i + 1u < n && known[0][i + 1u])
                misalign.push_back(path[1][i] - ((path[0][i] + path[0][i + 1u]) / 2.0));
        }
        else if (fo == AV_FIELD_BB || fo == AV_FIELD_BT) {
            if (i > 0u && known[0][i - 1u])
                misalign.push_back(path[1][i] - ((path[0][i - 1u] + path[0][i]) / 2.0));
        }
        else {
            misalign.push_back(path[1][i] - path[0][i]);
        }
    }

    if (misalign.size() < 5u)
        return false;

    // VHS jitter moves a field by whole lines
    d = floor(0.5 - median(misalign));
    return true;
}

static bool measure(const options_t &opt,video_info_t &vi,std::vector<frame_meas_t> &meas) {
    input_t in;

    if (!in.open(opt.input,true))
        return false;

    const AVStream *st = in.vstream();
    progress_t progress("Measuring",frame_count(in),opt.quiet);
    converter_t conv;
    AVFrame *work = NULL;
    tracker_t tracker;
    profile_t prof[2];
    unsigned int nf = 1;                            // pictures measured per frame: fields or frames
    unsigned int left = 0,right = 0;
    std::vector<profile_t> window;                  // profiles of the frames since the tracker started again
    size_t window_start = 0;                        // the frame it started again at
    bool anchoring = false;
    bool first = true;
    bool ok;

    // When the tracker starts again, at the first frame or when a frame matches nothing,
    // it takes the fields of that frame to be lined up as they were in the frame before, or
    // with each other for the first frame. With jitter, they may not be, and then each frame
    // made from them to match to is wrong. So once there are enough frames to tell how the
    // fields are lined up most of the time, those frames are measured again, from that.
    auto anchor = [&](void) {
        double d;

        if (window.size() >= 4u && field_offset(window,tracker.params,vi.field_order,d)) {
            frame_meas_t &m0 = meas[window_start];
            const double pos[2] = { m0.f[0].step.pos, m0.f[0].step.pos + d };

            if (fabs(pos[1] - m0.f[1].step.pos) > 0.05) {
                tracker_t::step_t steps[2];

                m0.f[1].step.pos = pos[1];
                tracker.restart_at(&window[0],pos);
                for (size_t i=1;i < window.size() / 2u;i++) {
                    tracker.add(&window[i * 2u],steps);
                    for (unsigned int p=0;p < 2u;p++)
                        meas[window_start + i].f[p].step = steps[p];
                }
            }
        }

        anchoring = false;
        window.clear();
    };

    vi.field_order = st->codecpar->field_order;
    vi.time_base = st->time_base;

    ok = in.run([&](AVFrame *frame) -> bool {
        if (first) {
            first = false;
            vi.width = frame->width;
            vi.height = frame->height;
            vi.dec_fmt = (enum AVPixelFormat)frame->format;
            vi.work_fmt = work_format_for(vi.dec_fmt);
            vi.sar = av_guess_sample_aspect_ratio(in.fmt,(AVStream*)st,frame);
            vi.color_range = frame->color_range != AVCOL_RANGE_UNSPECIFIED ? frame->color_range : st->codecpar->color_range;
            vi.color_primaries = frame->color_primaries != AVCOL_PRI_UNSPECIFIED ? frame->color_primaries : st->codecpar->color_primaries;
            vi.color_trc = frame->color_trc != AVCOL_TRC_UNSPECIFIED ? frame->color_trc : st->codecpar->color_trc;
            vi.colorspace = frame->colorspace != AVCOL_SPC_UNSPECIFIED ? frame->colorspace : st->codecpar->color_space;
            vi.chroma_location = frame->chroma_location != AVCHROMA_LOC_UNSPECIFIED ? frame->chroma_location : st->codecpar->chroma_location;
            if (vi.field_order == AV_FIELD_UNKNOWN && frame_is_interlaced(frame))
                vi.field_order = frame_is_tff(frame) ? AV_FIELD_TT : AV_FIELD_BB;

            // VHS is interlaced, so fields unless the file says otherwise
            if (opt.fields >= 0)
                vi.fields = opt.fields > 0;
            else
                vi.fields = vi.field_order != AV_FIELD_PROGRESSIVE;

            if (vi.work_fmt == AV_PIX_FMT_NONE) {
                fprintf(stderr,"%s: cannot work with %s pixels\n",opt.input.c_str(),av_get_pix_fmt_name(vi.dec_fmt));
                return false;
            }
            if (vi.work_fmt != vi.dec_fmt && (work=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL)
                return false;

            nf = vi.fields ? 2u : 1u;
            const unsigned int top = opt.margin[0] >= 0 ? (unsigned int)opt.margin[0] : (unsigned int)vi.height / 32u;
            const unsigned int bottom = opt.margin[1] >= 0 ? (unsigned int)opt.margin[1] : (unsigned int)vi.height / 16u;
            left = opt.margin[2] >= 0 ? (unsigned int)opt.margin[2] : (unsigned int)vi.width / 16u;
            right = opt.margin[3] >= 0 ? (unsigned int)opt.margin[3] : (unsigned int)vi.width / 16u;

            tracker.params.top = top;
            tracker.params.bottom = bottom;
            tracker.params.range = opt.range;
            tracker.params.min_score = opt.min_score;
            tracker.height = (unsigned int)vi.height;
            tracker.fields = nf;

            const int compared = vi.height - (int)top - (int)bottom - (2 * (int)opt.range);
            if (compared < 32) {
                fprintf(stderr,"%s: the picture is %d lines high, which leaves %d lines to compare after the margins and range: too few\n",
                    opt.input.c_str(),vi.height,compared);
                return false;
            }
            if (left + right + (PROFILE_STRIPS * 4u) > (unsigned int)vi.width) {
                fprintf(stderr,"%s: the left and right margins leave too little of the picture\n",opt.input.c_str());
                return false;
            }

            if (!opt.quiet) {
                fprintf(stderr,"%s: %dx%d %s %s, %s\n",opt.input.c_str(),vi.width,vi.height,
                    avcodec_get_name(st->codecpar->codec_id),av_get_pix_fmt_name(vi.dec_fmt),field_order_name(vi.field_order));
                fprintf(stderr,"Measuring %s, lines %u to %u of the frame and columns %u to %u\n",
                    vi.fields ? "each field on its own" : "whole frames",top,vi.height - bottom - 1u,left,vi.width - right - 1u);
            }
        }

        if (frame->width != vi.width || frame->height != vi.height) {
            fprintf(stderr,"\n%s: the frame size changes from %dx%d to %dx%d, which this cannot do\n",
                opt.input.c_str(),vi.width,vi.height,frame->width,frame->height);
            return false;
        }

        const AVFrame *wf = frame;
        if (frame->format != vi.work_fmt) {
            if (work == NULL && (work=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (!conv.convert(work,frame)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format),av_get_pix_fmt_name(vi.work_fmt));
                return false;
            }
            wf = work;
        }

        const plane_t pl = measure_plane(wf);
        tracker_t::step_t steps[2];
        frame_meas_t m;

        for (unsigned int p=0;p < nf;p++)
            make_profile(prof[p],pl,p,nf,PROFILE_STRIPS,left,right);
        tracker.add(prof,steps);

        m.pts = frame->best_effort_timestamp;
        for (unsigned int p=0;p < nf;p++)
            m.f[p].step = steps[p];
        meas.push_back(m);

        if (nf == 2u) {
            const bool fresh0 = steps[0].how == tracker_t::FIRST || steps[0].how == tracker_t::LOST;
            const bool fresh1 = steps[1].how == tracker_t::FIRST || steps[1].how == tracker_t::LOST;

            if (fresh0 && fresh1) {
                window.clear();
                window_start = meas.size() - 1u;
                anchoring = true;
            }
            if (anchoring) {
                window.push_back(prof[0]);
                window.push_back(prof[1]);
                if (window.size() >= ANCHOR_FRAMES * 2u)
                    anchor();
            }
        }
        progress.frame(meas.size());
        return true;
    },nullptr);

    progress.done(meas.size());
    av_frame_free(&work);

    if (!ok)
        return false;
    if (meas.empty()) {
        fprintf(stderr,"%s: no video frames\n",opt.input.c_str());
        return false;
    }
    if (anchoring)
        anchor();

    // where the picture should be: the path it takes, smoothed
    const size_t n = meas.size();
    std::vector<double> smooth[2];

    if (nf == 2u) {
        // How far apart the fields are: they are lined up with each other most of the time,
        // or as far apart as the picture moves from one field to the next, so the median of
        // that over a while, smoothed. Jitter that moves one field and not the other hardly
        // changes the median. This does not need to know which field comes first.
        std::vector<double> apart(n),both(n * 2u);
        const unsigned int mr = std::max(2u,(unsigned int)ceil(opt.radius));

        for (size_t i=0;i < n;i++)
            apart[i] = meas[i].f[1].step.pos - meas[i].f[0].step.pos;
        apart = median_path(apart,mr);
        if (opt.radius > 0)
            apart = smooth_path(apart,opt.radius,false);

        // and where the picture is between them, from both fields, smoothed together
        for (size_t i=0;i < n;i++) {
            both[i * 2u] = meas[i].f[0].step.pos + (apart[i] / 2.0);
            both[(i * 2u) + 1u] = meas[i].f[1].step.pos - (apart[i] / 2.0);
        }
        if (opt.radius > 0)
            both = smooth_path(both,opt.radius * 2.0);
        else
            both.assign(both.size(),median(both));

        smooth[0].resize(n);
        smooth[1].resize(n);
        for (size_t i=0;i < n;i++) {
            const double mid = (both[i * 2u] + both[(i * 2u) + 1u]) / 2.0;
            smooth[0][i] = mid - (apart[i] / 2.0);
            smooth[1][i] = mid + (apart[i] / 2.0);
        }
    }
    else {
        std::vector<double> path(n);

        for (size_t i=0;i < n;i++)
            path[i] = meas[i].f[0].step.pos;
        if (opt.radius > 0)
            smooth[0] = smooth_path(path,opt.radius);
        else
            smooth[0].assign(n,median(path));
    }

    // and the correction, from where the picture is to there
    const double max_shift = opt.max_shift >= 0 ? opt.max_shift : (double)opt.range;
    for (size_t i=0;i < n;i++) {
        for (unsigned int p=0;p < nf;p++) {
            field_meas_t &f = meas[i].f[p];
            f.smooth = smooth[p][i];
            f.shift = std::max(-max_shift,std::min(max_shift,f.smooth - f.step.pos));
            // as far as it will really move: whole lines, of the field if fields
            if (opt.interp == INTERP_NEAREST)
                f.shift = nf * floor((f.shift / nf) + 0.5);
        }
    }

    if (!opt.quiet) {
        unsigned long count[4] = {0,0,0,0};
        double total = 0,most = 0;

        for (size_t i=0;i < meas.size();i++) {
            for (unsigned int p=0;p < nf;p++) {
                count[meas[i].f[p].step.how]++;
                total += fabs(meas[i].f[p].shift);
                most = std::max(most,fabs(meas[i].f[p].shift));
            }
        }

        fprintf(stderr,"%lu frames: %lu %s matched to a key frame, %lu to the frame before, %lu could not be matched\n",
            (unsigned long)meas.size(),count[tracker_t::KEY],vi.fields ? "fields" : "frames",count[tracker_t::PREV],count[tracker_t::LOST]);
        fprintf(stderr,"Moving the picture %.2f lines on average, %.2f lines at most\n",total / (double)(meas.size() * nf),most);
    }

    return true;
}

static bool write_log(const options_t &opt,const video_info_t &vi,const std::vector<frame_meas_t> &meas) {
    static const char *how_names[] = { "first", "key", "prev", "lost" };
    static const char *field_names[2][2] = { { "frame", "" }, { "top", "bottom" } };
    const unsigned int nf = vi.fields ? 2u : 1u;
    FILE *fp = fopen(opt.log_path.c_str(),"w");

    if (fp == NULL) {
        fprintf(stderr,"%s: cannot write: %s\n",opt.log_path.c_str(),strerror(errno));
        return false;
    }

    // positions and shifts are in lines of the frame, down from where the first frame was
    fprintf(fp,"frame,time");
    for (unsigned int p=0;p < nf;p++) {
        const char *n = field_names[nf - 1u][p];
        fprintf(fp,",%s_pos,%s_smooth,%s_shift,%s_score,%s_match",n,n,n,n,n);
    }
    fprintf(fp,"\n");

    for (size_t i=0;i < meas.size();i++) {
        const frame_meas_t &m = meas[i];

        fprintf(fp,"%lu,",(unsigned long)i);
        if (m.pts != AV_NOPTS_VALUE)
            fprintf(fp,"%.6f",m.pts * av_q2d(vi.time_base));
        for (unsigned int p=0;p < nf;p++) {
            const field_meas_t &f = m.f[p];
            fprintf(fp,",%.3f,%.3f,%.3f,%.4f,%s",f.step.pos,f.smooth,f.shift,f.step.score,how_names[f.step.how]);
        }
        fprintf(fp,"\n");
    }

    if (fclose(fp) != 0) {
        fprintf(stderr,"%s: cannot write: %s\n",opt.log_path.c_str(),strerror(errno));
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// second pass: moving the picture and writing the output file

class output_t {
public:
    AVFormatContext*            oc = NULL;
    AVCodecContext*             enc = NULL;
    AVPacket*                   pkt = NULL;
    std::vector<int>            map;                // output stream of each input stream, or -1
    int                         vindex = -1;        // output video stream
    int64_t                     frame_duration = 0; // in the encoder time base
    int64_t                     last_pts = AV_NOPTS_VALUE;

    ~output_t() {
        if (oc != NULL) {
            if (!(oc->oformat->flags & AVFMT_NOFILE))
                avio_closep(&oc->pb);
            avformat_free_context(oc);
        }
        avcodec_free_context(&enc);
        av_packet_free(&pkt);
    }

    // write the packets the encoder has ready
    bool write_encoded(void) {
        for (;;) {
            int r = avcodec_receive_packet(enc,pkt);
            if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                return true;
            if (r < 0) {
                fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
                return false;
            }

            if (pkt->duration <= 0)
                pkt->duration = frame_duration;
            av_packet_rescale_ts(pkt,enc->time_base,oc->streams[vindex]->time_base);
            pkt->stream_index = vindex;
            if ((r=av_interleaved_write_frame(oc,pkt)) < 0) {
                fprintf(stderr,"\nError writing video: %s\n",averr(r).c_str());
                return false;
            }
        }
    }
};

static const AVCodec *choose_encoder(const options_t &opt,enum AVCodecID id) {
    const AVCodec *c;

    if (!opt.codec.empty())
        return avcodec_find_encoder_by_name(opt.codec.c_str());

    // prores_ks makes better ProRes than the default ProRes encoder, and can make it interlaced
    if (id == AV_CODEC_ID_PRORES && (c=avcodec_find_encoder_by_name("prores_ks")) != NULL)
        return c;
    if ((c=avcodec_find_encoder(id)) != NULL)
        return c;

    return NULL;
}

// true for the codecs that can code interlaced video as fields, which FFmpeg's encoders for
// them do if asked to with AV_CODEC_FLAG_INTERLACED_DCT. Others, such as MJPEG, refuse to open.
static bool codec_codes_fields(enum AVCodecID id) {
    return id == AV_CODEC_ID_PRORES || id == AV_CODEC_ID_H264 || id == AV_CODEC_ID_MPEG2VIDEO ||
           id == AV_CODEC_ID_MPEG4 || id == AV_CODEC_ID_DNXHD;
}

// the stream's codec tag, if the output format can have it for that codec
static unsigned int copy_codec_tag(const AVOutputFormat *ofmt,const AVCodecParameters *par) {
    unsigned int tag = 0;

    if (ofmt->codec_tag == NULL || av_codec_get_id(ofmt->codec_tag,par->codec_tag) == par->codec_id ||
        !av_codec_get_tag2(ofmt->codec_tag,par->codec_id,&tag))
        return par->codec_tag;

    return 0;
}

static bool open_output(const options_t &opt,const video_info_t &vi,input_t &in,output_t &out,enum AVPixelFormat &enc_fmt) {
    const AVStream *ist = in.vstream();
    const AVCodecParameters *ipar = ist->codecpar;
    int r;

    if ((r=avformat_alloc_output_context2(&out.oc,NULL,NULL,opt.output.c_str())) < 0 || out.oc == NULL) {
        if ((r=avformat_alloc_output_context2(&out.oc,NULL,"mov",opt.output.c_str())) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
    }
    if ((out.pkt=av_packet_alloc()) == NULL)
        return false;

    // the video encoder
    const AVCodec *codec = choose_encoder(opt,ipar->codec_id);
    if (codec == NULL && !opt.codec.empty()) {
        fprintf(stderr,"No video encoder named %s\n",opt.codec.c_str());
        return false;
    }
    if (codec == NULL) {
        if ((codec=avcodec_find_encoder_by_name("prores_ks")) == NULL) {
            fprintf(stderr,"No encoder for %s video, and no prores_ks: use -c to choose one\n",avcodec_get_name(ipar->codec_id));
            return false;
        }
        fprintf(stderr,"No encoder for %s video, using %s\n",avcodec_get_name(ipar->codec_id),codec->name);
    }

    // the same pixel format as the input, if the encoder can take it
    const enum AVPixelFormat *fmts = encoder_pix_fmts(codec);
    enc_fmt = vi.dec_fmt;
    if (fmts != NULL) {
        bool found = false;
        for (const enum AVPixelFormat *p=fmts;*p != AV_PIX_FMT_NONE;p++) {
            if (*p == vi.dec_fmt) found = true;
        }
        if (!found)
            enc_fmt = avcodec_find_best_pix_fmt_of_list(fmts,vi.dec_fmt,pixfmt_has_alpha(vi.dec_fmt) ? 1 : 0,NULL);
    }
    if (enc_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr,"The %s encoder takes no pixel format this can give it\n",codec->name);
        return false;
    }

    AVCodecContext *enc = out.enc = avcodec_alloc_context3(codec);
    if (enc == NULL)
        return false;

    const AVRational fr = av_guess_frame_rate(in.fmt,in.vstream(),NULL);
    const bool interlaced = field_order_interlaced(vi.field_order);

    enc->width = vi.width;
    enc->height = vi.height;
    enc->pix_fmt = enc_fmt;
    enc->sample_aspect_ratio = vi.sar;
    if (fr.num > 0 && fr.den > 0) {
        enc->framerate = fr;
        enc->time_base = av_inv_q(fr);
    }
    else {
        enc->time_base = ist->time_base;
    }
    enc->field_order = vi.field_order;
    enc->color_range = (pixfmt_is_yuvj(vi.dec_fmt) && !pixfmt_is_yuvj(enc_fmt)) ? AVCOL_RANGE_JPEG : vi.color_range;
    enc->color_primaries = vi.color_primaries;
    enc->color_trc = vi.color_trc;
    enc->colorspace = vi.colorspace;
    enc->chroma_sample_location = vi.chroma_location;
    enc->thread_count = 0;
    if (interlaced && codec_codes_fields(codec->id))
        enc->flags |= AV_CODEC_FLAG_INTERLACED_DCT | AV_CODEC_FLAG_INTERLACED_ME;
    if (out.oc->oformat->flags & AVFMT_GLOBALHEADER)
        enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (codec->id == ipar->codec_id) {
        if (opt.bit_rate <= 0 && ipar->bit_rate > 0)
            enc->bit_rate = ipar->bit_rate;

        // the same kind of ProRes or DNxHR
        const int profile = ipar->profile >= 0 ? ipar->profile : in.dec->profile;
        if (profile >= 0 && (codec->id == AV_CODEC_ID_PRORES || codec->id == AV_CODEC_ID_DNXHD))
            av_opt_set_int(enc->priv_data,"profile",profile,0);
    }
    if (opt.bit_rate > 0)
        enc->bit_rate = opt.bit_rate;

    AVDictionary *eopts = NULL;
    if (!opt.codec_opts.empty() && (r=av_dict_parse_string(&eopts,opt.codec_opts.c_str(),"=",":",0)) < 0) {
        fprintf(stderr,"-x %s: %s\n",opt.codec_opts.c_str(),averr(r).c_str());
        av_dict_free(&eopts);
        return false;
    }
    r = avcodec_open2(enc,codec,&eopts);
    for (const AVDictionaryEntry *e=NULL;(e=av_dict_get(eopts,"",e,AV_DICT_IGNORE_SUFFIX)) != NULL;)
        fprintf(stderr,"The %s encoder has no option %s\n",codec->name,e->key);
    av_dict_free(&eopts);
    if (r < 0) {
        fprintf(stderr,"Cannot open the %s encoder for %dx%d %s: %s (-c chooses another encoder)\n",
            codec->name,vi.width,vi.height,av_get_pix_fmt_name(enc_fmt),averr(r).c_str());
        return false;
    }
    if (fr.num > 0 && fr.den > 0)
        out.frame_duration = std::max((int64_t)1,av_rescale_q(1,av_inv_q(fr),enc->time_base));

    // the streams, in the same order as the input: the video, and the audio copied as it is.
    // A timecode track is made again from the video's timecode, which the video keeps.
    const bool has_timecode = av_dict_get(ist->metadata,"timecode",NULL,0) != NULL;
    unsigned int skipped = 0;
    out.map.assign(in.fmt->nb_streams,-1);
    for (unsigned int i=0;i < in.fmt->nb_streams;i++) {
        const AVStream *s = in.fmt->streams[i];
        AVStream *os;

        if ((int)i != in.vindex && s->codecpar->codec_type != AVMEDIA_TYPE_AUDIO) {
            if (!(has_timecode && s->codecpar->codec_type == AVMEDIA_TYPE_DATA && av_dict_get(s->metadata,"timecode",NULL,0) != NULL))
                skipped++;
            continue;
        }
        if ((os=avformat_new_stream(out.oc,NULL)) == NULL)
            return false;

        av_dict_copy(&os->metadata,s->metadata,0);
        os->disposition = s->disposition;
        out.map[i] = os->index;

        if ((int)i == in.vindex) {
            out.vindex = os->index;
            if ((r=avcodec_parameters_from_context(os->codecpar,enc)) < 0)
                return false;
            os->time_base = enc->time_base;
            os->avg_frame_rate = fr;
            os->sample_aspect_ratio = vi.sar;
            // it was encoded again, by something else
            av_dict_set(&os->metadata,"encoder",NULL,0);
        }
        else {
            if ((r=avcodec_parameters_copy(os->codecpar,s->codecpar)) < 0)
                return false;
            os->codecpar->codec_tag = copy_codec_tag(out.oc->oformat,s->codecpar);
            os->time_base = s->time_base;
        }
    }
    av_dict_copy(&out.oc->metadata,in.fmt->metadata,0);

    if (!opt.quiet) {
        fprintf(stderr,"Writing %s: %s %s, %u audio stream%s copied",opt.output.c_str(),codec->name,av_get_pix_fmt_name(enc_fmt),
            (unsigned int)(out.oc->nb_streams - 1u),out.oc->nb_streams == 2u ? "" : "s");
        if (has_timecode)
            fprintf(stderr,", timecode kept");
        if (skipped != 0)
            fprintf(stderr,", %u other stream%s left out",skipped,skipped == 1u ? "" : "s");
        fprintf(stderr,"\n");
    }

    if (!(out.oc->oformat->flags & AVFMT_NOFILE)) {
        if ((r=avio_open(&out.oc->pb,opt.output.c_str(),AVIO_FLAG_WRITE)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
    }
    if ((r=avformat_write_header(out.oc,NULL)) < 0) {
        fprintf(stderr,"%s: cannot write the header: %s\n",opt.output.c_str(),averr(r).c_str());
        return false;
    }

    return true;
}

static bool write_output(const options_t &opt,const video_info_t &vi,const std::vector<frame_meas_t> &meas) {
    input_t in;
    output_t out;
    enum AVPixelFormat enc_fmt = AV_PIX_FMT_NONE;

    if (!in.open(opt.input,false))
        return false;
    if (!open_output(opt,vi,in,out,enc_fmt))
        return false;

    const AVRational itb = in.vstream()->time_base;
    const bool interlaced = field_order_interlaced(vi.field_order);
    const bool tff = vi.field_order == AV_FIELD_TT || vi.field_order == AV_FIELD_TB;
    const bool full_range = vi.color_range == AVCOL_RANGE_JPEG || pixfmt_is_yuvj(vi.dec_fmt);
    const unsigned int nf = vi.fields ? 2u : 1u;
    progress_t progress("Writing",(int64_t)meas.size(),opt.quiet);
    converter_t conv_in,conv_out;
    AVFrame *work_in = NULL;
    unsigned long n = 0;
    bool warned_pts = false,warned_count = false;
    bool ok;

    ok = in.run([&](AVFrame *frame) -> bool {
        if (frame->width != vi.width || frame->height != vi.height) {
            fprintf(stderr,"\n%s: the frame size changes, which this cannot do\n",opt.input.c_str());
            return false;
        }

        // the correction measured for this frame
        double shift[2] = {0,0};
        if (n < meas.size()) {
            if (meas[n].pts != frame->best_effort_timestamp && !warned_pts) {
                fprintf(stderr,"\nFrame %lu is not the frame measured, the corrections may be wrong\n",n);
                warned_pts = true;
            }
            for (unsigned int p=0;p < nf;p++)
                shift[p] = meas[n].f[p].shift;
        }
        else if (!warned_count) {
            fprintf(stderr,"\nMore frames than were measured: frame %lu on is not corrected\n",n);
            warned_count = true;
        }

        const AVFrame *src = frame;
        if (frame->format != vi.work_fmt) {
            if (work_in == NULL && (work_in=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (!conv_in.convert(work_in,frame)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format),av_get_pix_fmt_name(vi.work_fmt));
                return false;
            }
            src = work_in;
        }

        // a new frame each time, since the encoder may hold on to the last one
        AVFrame *moved = alloc_frame(vi.work_fmt,vi.width,vi.height);
        if (moved == NULL)
            return false;

        const std::vector<frame_plane_t> sp = frame_planes(src,full_range);
        const std::vector<frame_plane_t> dp = frame_planes(moved,full_range);
        for (size_t i=0;i < sp.size();i++) {
            for (unsigned int p=0;p < nf;p++)
                shift_lines(dp[i].pl,sp[i].pl,p,nf,shift[p] / (double)(nf << sp[i].vsub),opt.interp,sp[i].fill);
        }

        AVFrame *ef = moved;
        if (vi.work_fmt != enc_fmt) {
            if ((ef=alloc_frame(enc_fmt,vi.width,vi.height)) == NULL) {
                av_frame_free(&moved);
                return false;
            }
            moved->color_range = frame->color_range;
            moved->colorspace = frame->colorspace;
            if (!conv_out.convert(ef,moved)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name(vi.work_fmt),av_get_pix_fmt_name(enc_fmt));
                av_frame_free(&moved);
                av_frame_free(&ef);
                return false;
            }
            av_frame_free(&moved);
        }

        av_frame_copy_props(ef,frame);
        ef->pict_type = AV_PICTURE_TYPE_NONE;
        ef->sample_aspect_ratio = vi.sar;
        if (out.enc->color_range == AVCOL_RANGE_JPEG)
            ef->color_range = AVCOL_RANGE_JPEG;
        if (interlaced && !frame_is_interlaced(ef))
            frame_set_interlaced(ef,tff);

        // timestamps in the encoder's time base, always going forward
        int64_t pts = frame->best_effort_timestamp;
        if (pts != AV_NOPTS_VALUE)
            pts = av_rescale_q(pts,itb,out.enc->time_base);
        else
            pts = (out.last_pts != AV_NOPTS_VALUE) ? (out.last_pts + std::max((int64_t)1,out.frame_duration)) : 0;
        if (out.last_pts != AV_NOPTS_VALUE && pts <= out.last_pts)
            pts = out.last_pts + 1;
        ef->pts = out.last_pts = pts;

        int r = avcodec_send_frame(out.enc,ef);
        av_frame_free(&ef);
        if (r < 0) {
            fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
            return false;
        }
        if (!out.write_encoded())
            return false;

        progress.frame(++n);
        return true;
    },[&](AVPacket *pkt) -> bool {
        // audio, as it is
        const int o = out.map[(size_t)pkt->stream_index];
        if (o < 0)
            return true;

        av_packet_rescale_ts(pkt,in.fmt->streams[pkt->stream_index]->time_base,out.oc->streams[o]->time_base);
        pkt->stream_index = o;
        pkt->pos = -1;

        int r = av_interleaved_write_frame(out.oc,pkt);
        if (r < 0) {
            fprintf(stderr,"\nError writing audio: %s\n",averr(r).c_str());
            return false;
        }
        return true;
    });

    progress.done(n);
    av_frame_free(&work_in);

    // what the encoder has left
    if (ok) {
        int r = avcodec_send_frame(out.enc,NULL);
        if (r < 0 && r != AVERROR_EOF) {
            fprintf(stderr,"Video encoding error: %s\n",averr(r).c_str());
            ok = false;
        }
        else {
            ok = out.write_encoded();
        }
    }

    int r = av_write_trailer(out.oc);
    if (r < 0) {
        fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
        ok = false;
    }
    if (!ok)
        fprintf(stderr,"%s is not complete\n",opt.output.c_str());

    return ok;
}

/////////////////////////////////////////////////////////////////////////////

static bool parse_double(const char *s,double &v) {
    char *end = NULL;
    v = strtod(s,&end);
    return end != s && *end == 0;
}

// a bit rate, such as 50000000, 50000k or 50M
static bool parse_rate(const char *s,int64_t &v) {
    char *end = NULL;
    double d = strtod(s,&end);

    if (end == s)
        return false;
    if (*end == 'k' || *end == 'K') { d *= 1e3; end++; }
    else if (*end == 'M') { d *= 1e6; end++; }
    else if (*end == 'G') { d *= 1e9; end++; }
    if (*end != 0 || d <= 0)
        return false;

    v = (int64_t)d;
    return true;
}

int main(int argc,char **argv) {
    std::vector<std::string> files;
    options_t opt;

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != 0) {
            const std::string o = a + 1;
            // options that take a value
            const bool takes = (o == "r" || o == "w" || o == "M" || o == "t" || o == "margin" || o == "i" ||
                                o == "c" || o == "b" || o == "x" || o == "log");
            const char *v = NULL;
            double d;

            if (takes) {
                if (++i >= argc) {
                    fprintf(stderr,"%s needs a value\n",a);
                    return 1;
                }
                v = argv[i];
            }

            if (o == "r") {
                if (!parse_double(v,d) || d < 1 || d > 1000 || d != floor(d)) {
                    fprintf(stderr,"-r %s: the range must be 1 to 1000 lines\n",v);
                    return 1;
                }
                opt.range = (unsigned int)d;
            }
            else if (o == "w") {
                if (!parse_double(v,d) || d < 0 || d > 100000) {
                    fprintf(stderr,"-w %s: the smoothing radius must be 0 or more frames\n",v);
                    return 1;
                }
                opt.radius = d;
            }
            else if (o == "M") {
                if (!parse_double(v,d) || d < 0) {
                    fprintf(stderr,"-M %s: must be 0 or more lines\n",v);
                    return 1;
                }
                opt.max_shift = d;
            }
            else if (o == "t") {
                if (!parse_double(v,d) || d < -1 || d > 1) {
                    fprintf(stderr,"-t %s: the score must be 0 to 1\n",v);
                    return 1;
                }
                opt.min_score = d;
            }
            else if (o == "margin") {
                int m[4];
                char extra;
                if (sscanf(v,"%d,%d,%d,%d%c",&m[0],&m[1],&m[2],&m[3],&extra) != 4 || m[0] < 0 || m[1] < 0 || m[2] < 0 || m[3] < 0) {
                    fprintf(stderr,"-margin %s: give the top, bottom, left and right margins, such as 16,32,40,40\n",v);
                    return 1;
                }
                for (int j=0;j < 4;j++) opt.margin[j] = m[j];
            }
            else if (o == "i") {
                const std::string s = v;
                if (s == "nearest") opt.interp = INTERP_NEAREST;
                else if (s == "linear") opt.interp = INTERP_LINEAR;
                else if (s == "cubic") opt.interp = INTERP_CUBIC;
                else {
                    fprintf(stderr,"-i %s: must be cubic, linear or nearest\n",v);
                    return 1;
                }
            }
            else if (o == "c") {
                opt.codec = v;
            }
            else if (o == "b") {
                if (!parse_rate(v,opt.bit_rate)) {
                    fprintf(stderr,"-b %s: give the bit rate as a number, such as 50000000, 50000k or 50M\n",v);
                    return 1;
                }
            }
            else if (o == "x") {
                if (!opt.codec_opts.empty()) opt.codec_opts += ":";
                opt.codec_opts += v;
            }
            else if (o == "log") {
                opt.log_path = v;
            }
            else if (o == "I") {
                opt.fields = 1;
            }
            else if (o == "P") {
                opt.fields = 0;
            }
            else if (o == "y") {
                opt.overwrite = true;
            }
            else if (o == "n") {
                opt.measure_only = true;
            }
            else if (o == "q") {
                opt.quiet = true;
            }
            else if (o == "h" || o == "-help") {
                help();
                return 0;
            }
            else {
                fprintf(stderr,"Unknown option %s\n",a);
                help();
                return 1;
            }
        }
        else {
            files.push_back(a);
        }
    }

    if (files.size() != (opt.measure_only ? 1u : 2u)) {
        help();
        return 1;
    }
    opt.input = files[0];
    if (!opt.measure_only) {
        opt.output = files[1];
        if (same_file(opt.input,opt.output)) {
            fprintf(stderr,"%s: the output would write over the input\n",opt.output.c_str());
            return 1;
        }
        if (!opt.overwrite && file_exists(opt.output)) {
            fprintf(stderr,"%s already exists (-y writes over it)\n",opt.output.c_str());
            return 1;
        }
    }

    av_log_set_level(opt.quiet ? AV_LOG_ERROR : AV_LOG_WARNING);

    video_info_t vi;
    std::vector<frame_meas_t> meas;

    if (!measure(opt,vi,meas))
        return 1;

    if (!opt.log_path.empty() && !write_log(opt,vi,meas))
        return 1;

    if (!opt.measure_only && !write_output(opt,vi,meas))
        return 1;

    return 0;
}
