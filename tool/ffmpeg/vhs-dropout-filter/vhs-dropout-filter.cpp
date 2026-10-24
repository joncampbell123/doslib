/* vhs-dropout-filter: find the dropouts in video captured from a VHS tape, and fill them in,
 * or make holes of them.
 *
 * A dropout is where the head lost the signal on the tape for a moment, from a speck of dust,
 * a crease, or where the oxide has come off. It is a line, or part of a line, or a few lines
 * of the picture: the line before it repeated, by the dropout compensator of the VCR, or
 * static, white or black streaks and noise. It lasts a field, and the next field has it
 * somewhere else, or not at all.
 *
 * Each picture (field, or frame) is matched to the pictures before and after it, moved to where
 * it is. A dropout is a run of pixels across a line that is in none of them, and that looks like
 * a dropout, not like something that moved some other way than the pictures around it.
 *
 * -mode 1 fills in each dropout with what was last there that was not a dropout, the picture
 *         holding still there, as a MiniDV deck does with its dropouts. It is the quickest.
 * -mode 2 makes the dropouts holes in the picture, an alpha channel in ProRes 4444, to fill in
 *         with another copy of the tape on a track under it in an editor.
 * -mode 3 fills in each dropout from the pictures before and after it, moved to where it is.
 *
 * The video is read once, a few pictures ahead of the one being written. The audio is copied as it is. */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#include <algorithm>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/avconfig.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include <omp.h>

#include "plane.h"
#include "motion.h"
#include "detect.h"
#include "fill.h"

enum filter_mode_t {
    MODE_HOLD=1,                                    // fill in with what was last there
    MODE_ALPHA=2,                                   // make holes of them
    MODE_MOTION=3                                   // fill in from the pictures around, moved to where it is
};

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 log_path;
    std::string                 codec;              // video encoder name
    std::string                 codec_opts;         // key=value:key=value
    int64_t                     bit_rate = 0;
    int                         mode = 0;
    int                         near = -1;          // pictures before and after to compare with, -1 for the default of the mode
    int                         margin[4] = {-1,-1,-1,-1};  // top, bottom, left, right; -1 for the default
    double                      feather = 0;        // -mode 2: pixels the edge of a hole fades over
    double                      cut = 24;           // -mode 1: levels the picture changes by at a cut
    int64_t                     start_us = 0;       // -ss: how far into the input to start, in microseconds
    int64_t                     length_us = -1;     // -t: how much of it to do, -1 for all of it
    int                         threads = 0;        // to work on the pictures with, 0 for half the CPUs
    detect_params_t             dp;
    fill_params_t               fp;
    int                         fields = -1;        // -1 to decide from the file, 0 progressive, 1 interlaced
    bool                        show = false;       // mark the dropouts instead
    bool                        overwrite = false;
    bool                        quiet = false;
};

static void help(void) {
    fprintf(stderr,"vhs-dropout-filter -mode <1|2|3> [options] <input.mov> <output.mov>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Finds the dropouts in video captured from a VHS tape: lines, or parts of lines, of\n");
    fprintf(stderr,"static, or of the line above repeated by the VCR, that are there for one field and\n");
    fprintf(stderr,"gone the next. The video is encoded again, the audio is copied as it is.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -mode 1        Fill in each dropout with what was last there that was not a dropout,\n");
    fprintf(stderr,"                 that part of the picture holding still, as a MiniDV deck does. Quickest.\n");
    fprintf(stderr,"  -mode 2        Make holes of the dropouts: the output is ProRes 4444 with an alpha\n");
    fprintf(stderr,"                 channel, clear where the dropouts are, to put over another copy of the\n");
    fprintf(stderr,"                 tape in an editor such as Premiere, so that the holes show that copy.\n");
    fprintf(stderr,"  -mode 3        Fill in each dropout from the pictures before and after it, following\n");
    fprintf(stderr,"                 how the picture moves, made as bright as the picture around it.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Finding dropouts:\n");
    fprintf(stderr,"  -level <n>     The least a dropout differs from the pictures around it, in levels\n");
    fprintf(stderr,"                 of 0 to 255 (default 10). Less finds more, and more that are not dropouts.\n");
    fprintf(stderr,"  -noise <n>     Or this many times as much as the picture differs from them as a rule,\n");
    fprintf(stderr,"                 if more, for noisy or fast moving video (default 4)\n");
    fprintf(stderr,"  -local <n>     Or this many times as much as the lines above and below it differ from them,\n");
    fprintf(stderr,"                 and half that of the same place in the pictures before it, if more, so\n");
    fprintf(stderr,"                 that water, leaves and crowds, never the same twice, are not dropouts\n");
    fprintf(stderr,"                 (default 3). Less finds more in them; 0 does not look at them.\n");
    fprintf(stderr,"  -strong <n>    A line that differs this many times as much, where the lines a few above\n");
    fprintf(stderr,"                 and below it do not, need not look like a dropout, a line repeated or a\n");
    fprintf(stderr,"                 line of static, to be one (default 3)\n");
    fprintf(stderr,"  -run <pixels>  The shortest dropout across a line (default 16)\n");
    fprintf(stderr,"  -grow <x>,<y>  Pixels each side, and lines above and below, put in with each dropout,\n");
    fprintf(stderr,"                 that it smears into (default 3,2)\n");
    fprintf(stderr,"  -near <n>      Pictures before and after to compare with, and with -mode 3 to fill in\n");
    fprintf(stderr,"                 from, 1 to 4 (default 1 for -mode 1, 2 otherwise)\n");
    fprintf(stderr,"  -area <frac>   If more than this much of a picture, 0 to 1, differs from the pictures\n");
    fprintf(stderr,"                 around it, it is a flash or the like, not dropouts (default 0.6)\n");
    fprintf(stderr,"  -margin <top>,<bottom>,<left>,<right>\n");
    fprintf(stderr,"                 Lines and pixels at the edges that are never dropouts, such as the head\n");
    fprintf(stderr,"                 switching noise at the bottom (default: 1/48 of the height at the bottom\n");
    fprintf(stderr,"                 and the top, none at the sides)\n");
    fprintf(stderr,"  -I             Interlaced: each field is a picture of its own\n");
    fprintf(stderr,"                 (default, unless the file says the video is progressive)\n");
    fprintf(stderr,"  -P             Progressive: each frame is a picture\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Filling in:\n");
    fprintf(stderr,"  -cut <levels>  -mode 1: where the picture changes more than this from what is held,\n");
    fprintf(stderr,"                 a cut, the dropouts are filled from the lines above and below (default 24)\n");
    fprintf(stderr,"  -nomatch       -mode 3: do not make what is filled in as bright as the picture around it\n");
    fprintf(stderr,"  -feather <px>  -mode 2: the edges of the holes fade over this many pixels (default 0)\n");
    fprintf(stderr,"  -show          Mark the dropouts in magenta instead, to see what is found\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Output:\n");
    fprintf(stderr,"  -c <encoder>   Video encoder, such as prores_ks, ffv1 or libx264 (default: the input's\n");
    fprintf(stderr,"                 codec, with prores_ks for ProRes; prores_ks 4444 for -mode 2)\n");
    fprintf(stderr,"  -b <rate>      Video bit rate, such as 50M (default: the input's, if the codec is the same)\n");
    fprintf(stderr,"  -x <k=v:k=v>   Video encoder options, such as -x profile=5 (ProRes 4444 XQ for prores_ks)\n");
    fprintf(stderr,"  -ss <time>     Start this far into the input, as FFmpeg's -ss: seconds, such as 90.5,\n");
    fprintf(stderr,"                 or [HH:]MM:SS[.m...], such as 1:30.5. The output starts at 0.\n");
    fprintf(stderr,"  -t <time>      Do only this much of the input, as FFmpeg's -t\n");
    fprintf(stderr,"  -y             Write over the output file if it exists\n");
    fprintf(stderr,"  -log <file>    Write the dropouts found in each frame to a CSV file\n");
    fprintf(stderr,"  -threads <n>   Threads to work on the pictures with (default: half the CPUs, leaving\n");
    fprintf(stderr,"                 the rest to decode and encode the video)\n");
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

// set by CTRL+C: stop reading the input, and finish the output file with what there is
static volatile sig_atomic_t interrupted = 0;

static void on_interrupt(int sig) {
    (void)sig;
    interrupted = 1;
}

// CTRL+C stops the reading, so that what is written so far is finished and playable.
// It is only caught once: CTRL+C again quits at once.
static void catch_interrupt(void) {
    struct sigaction sa;

    memset(&sa,0,sizeof(sa));
    sa.sa_handler = on_interrupt;
    sa.sa_flags = SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,&sa,NULL);
}

// a time as FFmpeg takes it for -ss and -t: seconds, such as 90 or 1.5, or [HH:]MM:SS[.m...],
// in microseconds, or false if it is not one or is less than 0
static bool parse_time(const char *s,int64_t &us) {
    return av_parse_time(&us,s,1) >= 0 && us >= 0;
}

class input_t {
public:
    AVFormatContext*            fmt = NULL;
    AVCodecContext*             dec = NULL;
    int                         vindex = -1;
    int64_t                     start_us = 0;       // how far into the input to start, as FFmpeg's -ss
    int64_t                     length_us = -1;     // how much of it, as FFmpeg's -t, -1 for the rest of it
    const char*                 stop_message = NULL;    // said when CTRL+C stops the reading
    bool                        stopped = false;    // CTRL+C stopped the reading before the end

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

        // the start of the part to read, as FFmpeg's -ss: the keyframe at or before it, from which
        // the frames before it are decoded and left out
        t0 = fmt->start_time != AV_NOPTS_VALUE ? fmt->start_time : 0;
        const AVRational fr = av_guess_frame_rate(fmt,fmt->streams[vindex],NULL);
        half_frame_us = (fr.num > 0 && fr.den > 0) ? av_rescale_q(1,av_inv_q(fr),av_make_q(1,AV_TIME_BASE)) / 2 : 0;
        if (start_us > 0) {
            const int64_t ts = t0 + start_us;
            if ((r=avformat_seek_file(fmt,-1,INT64_MIN,ts,ts,0)) < 0) {
                fprintf(stderr,"%s: cannot go to %.3f seconds in: %s\n",path.c_str(),(double)start_us / AV_TIME_BASE,averr(r).c_str());
                return false;
            }
        }

        return true;
    }

    // what to take off the time of everything written, in microseconds, so that with -ss
    // the output starts at 0, as FFmpeg's does
    int64_t offset_us(void) const {
        return start_us > 0 ? t0 + start_us : 0;
    }

    // how far a frame or packet at ts in time base tb is into the input, from its start, in microseconds
    int64_t time_us(int64_t ts,AVRational tb) const {
        return av_rescale_q(ts,tb,av_make_q(1,AV_TIME_BASE)) - t0;
    }

    // read the file, or the part of it from start_us for length_us: decode each frame of the video
    // and give it to on_frame, and give each packet of the other streams to on_packet. Either can
    // stop it by returning false, which is an error. CTRL+C stops it too, but not as an error.
    bool run(const std::function<bool(AVFrame*)> &on_frame,const std::function<bool(AVPacket*)> &on_packet) {
        AVPacket *pkt = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        const AVRational vtb = fmt->streams[vindex]->time_base;
        const int64_t end_us = length_us >= 0 ? start_us + length_us : INT64_MAX;
        unsigned int errors = 0;
        bool ok = (pkt != NULL && frame != NULL);
        bool ended = false;                         // past the end of the part to read
        int r;

        // give on_frame each frame the decoder has ready, of the part to read: the frame nearest
        // the start, and the frames before the end
        auto drain = [&](void) -> bool {
            for (;;) {
                r = avcodec_receive_frame(dec,frame);
                if (r == AVERROR(EAGAIN) || r == AVERROR_EOF)
                    return true;
                if (r < 0) {
                    if (errors++ < 10u) fprintf(stderr,"\nVideo decoding error: %s\n",averr(r).c_str());
                    return true;
                }

                if (frame->best_effort_timestamp != AV_NOPTS_VALUE) {
                    const int64_t t = time_us(frame->best_effort_timestamp,vtb) + half_frame_us;
                    if (t < start_us) {
                        av_frame_unref(frame);
                        continue;
                    }
                    if (t >= end_us) {
                        av_frame_unref(frame);
                        ended = true;
                        return false;
                    }
                }

                const bool keep = on_frame(frame);
                av_frame_unref(frame);
                if (!keep)
                    return false;
            }
        };

        while (ok) {
            if (interrupted) {
                stopped = true;
                if (stop_message != NULL)
                    fprintf(stderr,"\n%s\n",stop_message);
                break;
            }

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
                // of the part to read
                const AVStream *s = fmt->streams[pkt->stream_index];
                const int64_t t = pkt->pts != AV_NOPTS_VALUE ? time_us(pkt->pts,s->time_base) : start_us;
                if (t >= start_us && t < end_us)
                    ok = on_packet(pkt);
            }

            av_packet_unref(pkt);
        }

        // and the frames the decoder still has, unless stopped before the end, when some of them
        // may be missing frames they need, and are left out, which would put the others out of step
        if (ended || stopped) {
            ok = ok || ended;
        }
        else if (ok) {
            avcodec_send_packet(dec,NULL);
            ok = drain();
            if (ended)
                ok = true;
        }

        av_packet_free(&pkt);
        av_frame_free(&frame);
        return ok;
    }
private:
    int64_t                     t0 = 0;             // the time the input starts at, in microseconds
    int64_t                     half_frame_us = 0;
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

// how many frames the video stream has, or about how many, or 0 if not known,
// of the part of it that is read
static int64_t frame_count(const input_t &in) {
    const AVStream *st = in.vstream();
    const AVRational fr = av_guess_frame_rate(in.fmt,(AVStream*)st,NULL);
    const bool rate = fr.num > 0 && fr.den > 0;
    int64_t n = 0;

    if (st->nb_frames > 0)
        n = st->nb_frames;
    else if (in.fmt->duration > 0 && rate)
        n = av_rescale_q(in.fmt->duration,av_make_q(1,AV_TIME_BASE),av_inv_q(fr));

    if (rate && in.length_us >= 0)
        return av_rescale_q(in.length_us,av_make_q(1,AV_TIME_BASE),av_inv_q(fr));
    if (rate && in.start_us > 0 && n > 0)
        return std::max((int64_t)0,n - av_rescale_q(in.start_us,av_make_q(1,AV_TIME_BASE),av_inv_q(fr)));

    return n;
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

/////////////////////////////////////////////////////////////////////////////
// pictures

// what the video is like
struct video_info_t {
    int                         width = 0;
    int                         height = 0;
    enum AVPixelFormat          dec_fmt = AV_PIX_FMT_NONE;     // as decoded
    enum AVPixelFormat          work_fmt = AV_PIX_FMT_NONE;    // as worked on
    AVRational                  sar = {0,1};
    AVRational                  frame_rate = {0,1};
    enum AVColorRange           color_range = AVCOL_RANGE_UNSPECIFIED;
    enum AVColorPrimaries       color_primaries = AVCOL_PRI_UNSPECIFIED;
    enum AVColorTransferCharacteristic color_trc = AVCOL_TRC_UNSPECIFIED;
    enum AVColorSpace           colorspace = AVCOL_SPC_UNSPECIFIED;
    enum AVChromaLocation       chroma_location = AVCHROMA_LOC_UNSPECIFIED;
    enum AVFieldOrder           field_order = AV_FIELD_UNKNOWN;
    bool                        fields = true;      // each field is a picture of its own
    bool                        tff = true;         // the top field is shown first
    bool                        rgb = false;
};

// a color component of a frame of a workable format
struct comp_t {
    int                         plane = 0;
    unsigned int                hsub = 0;
    unsigned int                vsub = 0;
    unsigned int                depth = 8;
};

// the components of a workable format that are not alpha, in order: Y, U, V, or R, G, B, or Y
static std::vector<comp_t> color_comps(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    const bool rgb = !!(d->flags & AV_PIX_FMT_FLAG_RGB);
    const unsigned int n = d->nb_components - ((d->flags & AV_PIX_FMT_FLAG_ALPHA) ? 1u : 0u);
    std::vector<comp_t> r;

    for (unsigned int i=0;i < n;i++) {
        const bool chroma = !rgb && (i == 1u || i == 2u);
        comp_t c;

        c.plane = d->comp[i].plane;
        c.hsub = chroma ? d->log2_chroma_w : 0u;
        c.vsub = chroma ? d->log2_chroma_h : 0u;
        c.depth = (unsigned int)d->comp[i].depth;
        r.push_back(c);
    }

    return r;
}

// one picture: a field, or a whole frame
struct pic_t {
    int64_t                     seq = 0;            // in the order shown
    int64_t                     frame = 0;          // the frame it is of
    unsigned int                field = 0;          // the lines of the frame it is, 0 for the even lines (or the whole frame), 1 the odd
    std::vector<plane16_t>      pl;                 // its color components
    image_t                     lum;
    pyramid_t                   pyr;
    std::map<int,mvfield_t>     mv;                 // how the picture moved to the picture this many after it (before, if negative)
    mask_t                      mask;
    std::vector<float>          E;                  // how much each pixel differs from the pictures around it
    detect_stats_t              ds;
};

// a frame waiting for its pictures to be done
struct out_frame_t {
    int64_t                     index = 0;
    int64_t                     pts = AV_NOPTS_VALUE;   // in the input's time base
    AVFrame*                    work = NULL;        // the frame, of the work format, the pictures are put back in
    mask_t                      mask;               // its dropouts, of its luma
    unsigned int                pictures_left = 1;
    size_t                      pixels = 0;
    size_t                      temporal = 0,spatial = 0;
    float                       threshold = 0;
    float                       noise = 0;
    unsigned int                too_much = 0;
};

// the luma, or for RGB, green, which is most of the brightness
static unsigned int luma_index(const video_info_t &vi) {
    return vi.rgb ? 1u : 0u;
}

// take a picture out of a frame of the work format
static void get_picture(pic_t &p,const AVFrame *f,const video_info_t &vi,unsigned int field) {
    const std::vector<comp_t> comps = color_comps((enum AVPixelFormat)f->format);
    const unsigned int nf = vi.fields ? 2u : 1u;

    p.field = field;
    p.pl.resize(comps.size());
    for (size_t i=0;i < comps.size();i++) {
        const comp_t &c = comps[i];
        const unsigned int w = (unsigned int)AV_CEIL_RSHIFT(f->width,(int)c.hsub);
        const unsigned int fh = (unsigned int)AV_CEIL_RSHIFT(f->height,(int)c.vsub);
        const unsigned int h = nf == 2u ? ((fh + 1u - field) / 2u) : fh;
        const unsigned int shift = 16u - c.depth;
        plane16_t &pl = p.pl[i];

        pl.alloc(w,h);
        pl.hsub = c.hsub;
        pl.vsub = c.vsub;
        for (unsigned int y=0;y < h;y++) {
            const uint8_t *s = f->data[c.plane] + ((ptrdiff_t)((y * nf) + field) * f->linesize[c.plane]);
            uint16_t *d = pl.line(y);
            if (c.depth > 8u) {
                const uint16_t *s16 = (const uint16_t*)s;
                for (unsigned int x=0;x < w;x++)
                    d[x] = (uint16_t)(s16[x] << shift);
            }
            else {
                for (unsigned int x=0;x < w;x++)
                    d[x] = (uint16_t)(s[x] << shift);
            }
        }
    }

    const plane16_t &l = p.pl[luma_index(vi)];
    p.lum.alloc((int)l.width,(int)l.height);
    for (size_t i=0;i < l.v.size();i++)
        p.lum.v[i] = (float)l.v[i] * (1.0f / 256.0f);
}

// and put it back
static void put_picture(AVFrame *f,const std::vector<plane16_t> &pl,const video_info_t &vi,unsigned int field) {
    const std::vector<comp_t> comps = color_comps((enum AVPixelFormat)f->format);
    const unsigned int nf = vi.fields ? 2u : 1u;

    for (size_t i=0;i < comps.size() && i < pl.size();i++) {
        const comp_t &c = comps[i];
        const unsigned int shift = 16u - c.depth;
        const unsigned int round = shift > 0u ? (1u << (shift - 1u)) : 0u;
        const unsigned int maxv = (1u << c.depth) - 1u;

        for (unsigned int y=0;y < pl[i].height;y++) {
            uint8_t *d = f->data[c.plane] + ((ptrdiff_t)((y * nf) + field) * f->linesize[c.plane]);
            const uint16_t *s = pl[i].line(y);
            if (c.depth > 8u) {
                uint16_t *d16 = (uint16_t*)d;
                for (unsigned int x=0;x < pl[i].width;x++)
                    d16[x] = (uint16_t)std::min(maxv,((unsigned int)s[x] + round) >> shift);
            }
            else {
                for (unsigned int x=0;x < pl[i].width;x++)
                    d[x] = (uint8_t)std::min(maxv,((unsigned int)s[x] + round) >> shift);
            }
        }
    }
}

// the dropouts in magenta, for -show
static void mark_dropouts(std::vector<plane16_t> &pl,const mask_t &mask,const video_info_t &vi) {
    const plane16_t &l = pl[luma_index(vi)];

    for (size_t i=0;i < pl.size();i++) {
        mask_t pm;
        plane_mask(pm,mask,(int)l.width,(int)l.height,pl[i]);

        uint16_t v;
        if (pl.size() < 3u)
            v = 65535;                              // gray: white
        else if (vi.rgb)
            v = i == 1u ? 0 : 65535;                // R and B
        else if (i == 0u)
            continue;                               // Y as it is
        else
            v = 56000;                              // U and V well above the middle

        for (size_t j=0;j < pm.size();j++) {
            if (pm[j])
                pl[i].v[j] = v;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
// working through the pictures, a few behind the last one read

class pipeline_t {
public:
    pipeline_t(const options_t &opt,const video_info_t &vi) : opt(opt), vi(vi) {
        near = opt.near > 0 ? opt.near : (opt.mode == MODE_HOLD ? 1 : 2);

        // the margins and the lines a dropout grows by, in lines of the frame, for the size of the video
        dp = opt.dp;
        if (dp.margin[0] < 0) dp.margin[0] = vi.height / 48;
        if (dp.margin[1] < 0) dp.margin[1] = vi.height / 48;
        if (dp.grow_y < 0) dp.grow_y = 2;
        if (vi.fields) {
            dp.margin[0] = (dp.margin[0] + 1) / 2;
            dp.margin[1] = (dp.margin[1] + 1) / 2;
            dp.grow_y = (dp.grow_y + 1) / 2;
            dp.local_lines = (dp.local_lines + 1) / 2;
        }

        if (vi.fields) {
            // the picture moves half as many lines in a field
            mp.range_y /= 2;
            mp.block = 16;
        }
    }

    ~pipeline_t() {
        for (auto &f : frames)
            av_frame_free(&f.second.work);
    }

    // done with a frame: all its pictures are filled in
    std::function<bool(out_frame_t&)> on_frame;

    // add the next frame, of the work format, which this keeps
    bool add(AVFrame *work,int64_t pts) {
        const unsigned int nf = vi.fields ? 2u : 1u;
        out_frame_t &of = frames[next_frame];

        of.index = next_frame;
        of.pts = pts;
        of.work = work;
        of.mask.assign((size_t)vi.width * vi.height,0);
        of.pictures_left = nf;

        for (unsigned int i=0;i < nf;i++) {
            std::unique_ptr<pic_t> p(new pic_t);
            // the field shown first, then the other
            const unsigned int field = nf == 2u ? (vi.tff ? i : (1u - i)) : 0u;

            get_picture(*p,work,vi,field);
            build_pyramid(p->pyr,p->lum,4);
            p->seq = next_seq++;
            p->frame = next_frame;
            pics.push_back(std::move(p));
            arrived(next_seq - 1);
        }

        next_frame++;
        return pump(false);
    }

    // the end of the video: do what is left
    bool finish(void) {
        return pump(true);
    }

    size_t                      total_pixels = 0;   // of dropouts, over the whole video
    size_t                      total_temporal = 0;
    size_t                      total_spatial = 0;
    unsigned long               flashes = 0;        // pictures with too much differing to be dropouts
    unsigned long               with_dropouts = 0;  // pictures with dropouts
    unsigned long               cuts = 0;           // -mode 1: pictures not filled in from what was held, at a cut
private:
    pic_t *get(int64_t seq) {
        if (pics.empty() || seq < pics.front()->seq || seq > pics.back()->seq)
            return NULL;
        return pics[(size_t)(seq - pics.front()->seq)].get();
    }

    bool pump(bool eof) {
        const int64_t last = next_seq - 1;

        while (next_detect <= last && (eof || next_detect + near <= last))
            detect(next_detect++);
        while (next_fill < next_detect && (eof || next_fill + near < next_detect)) {
            if (!fill(next_fill++))
                return false;
        }
        while (!pics.empty() && pics.front()->seq < next_fill - near)
            pics.pop_front();

        return true;
    }

    // a picture came: how it moved from each of the pictures before it it is compared with
    void arrived(int64_t seq) {
        pic_t &c = *get(seq);

        for (int k=1;k <= near;k++) {
            pic_t *b = get(seq - k);
            if (b != NULL && !b->pyr.empty())
                estimate_motion(c.mv[-k],c.pyr,b->pyr,mp);
        }

        // the pyramid of the picture furthest before it is not matched to again
        pic_t *old = get(seq - near);
        if (old != NULL)
            old->pyr.clear();
    }

    void detect(int64_t seq) {
        pic_t &c = *get(seq);
        std::vector<image_t> preds;
        std::vector<const image_t*> pp;

        preds.reserve((size_t)near * 2u);
        for (int k=-near;k <= near;k++) {
            pic_t *n = k != 0 ? get(seq + k) : NULL;
            if (n == NULL)
                continue;

            // how it moved to the pictures before it was found when it came, and to the pictures after it,
            // when they came, the other way
            mvfield_t &mv = c.mv[k];
            if (k > 0)
                n->mv[-k].reverse(mv);
            preds.push_back(image_t());
            predict(preds.back(),n->lum,mv);
        }
        for (size_t i=0;i < preds.size();i++)
            pp.push_back(&preds[i]);

        // how much the pictures just before it differed from the pictures around them
        std::vector<const std::vector<float>*> pe;
        for (int k=1;k <= 2;k++) {
            pic_t *b = get(seq - k);
            if (b != NULL && !b->E.empty())
                pe.push_back(&b->E);
        }

        detect_dropouts(c.mask,c.E,c.lum,pp,pe,dp,c.ds);

    }

    bool fill(int64_t seq) {
        pic_t &c = *get(seq);
        const unsigned int li = luma_index(vi);
        const int lw = (int)c.pl[li].width,lh = (int)c.pl[li].height;
        std::vector<plane16_t> out;
        fill_stats_t fs;

        if (c.ds.too_much)
            flashes++;
        if (c.ds.pixels != 0u)
            with_dropouts++;

        if (opt.show || opt.mode == MODE_ALPHA || c.ds.pixels == 0u) {
            out = c.pl;
        }
        else if (opt.mode == MODE_HOLD) {
            std::vector<plane16_t> &h = held[c.field];
            const std::vector<plane16_t> *from = h.empty() ? NULL : &h;

            // not what was before a cut
            if (from != NULL) {
                const plane16_t &a = c.pl[li],&b = h[li];
                double sum = 0;
                size_t count = 0;
                for (size_t i=0;i < a.v.size();i += 3) {
                    if (!c.mask[i]) {
                        sum += fabs((double)a.v[i] - (double)b.v[i]);
                        count++;
                    }
                }
                if (count != 0u && (sum / (double)count) / 256.0 > opt.cut) {
                    from = NULL;
                    cuts++;
                }
            }

            fill_hold(out,c.pl,c.mask,lw,lh,from,fs);
        }
        else {
            std::vector<mvfield_t> mvs;
            std::vector<fill_source_t> src;

            mvs.reserve((size_t)near * 2u);
            for (int k=-near;k <= near;k++) {
                pic_t *n = k != 0 ? get(seq + k) : NULL;
                if (n == NULL)
                    continue;

                mvs.push_back(c.mv[k]);
                refine_masked(mvs.back(),c.lum,n->lum,c.mask,n->mask,mp);

                fill_source_t s;
                s.pl = &n->pl;
                s.mask = &n->mask;
                s.mv = &mvs.back();
                s.distance = abs(k);
                src.push_back(s);
            }

            fill_motion(out,c.pl,c.mask,lw,lh,src,opt.fp,fs);
        }

        // -mode 1 holds on to what it filled in too, until there is something else there
        if (opt.mode == MODE_HOLD)
            held[c.field] = out;

        if (opt.show)
            mark_dropouts(out,c.mask,vi);

        // back into its frame
        auto it = frames.find(c.frame);
        out_frame_t &of = it->second;
        const unsigned int nf = vi.fields ? 2u : 1u;

        put_picture(of.work,out,vi,c.field);
        for (int y=0;y < lh;y++)
            memcpy(&of.mask[(size_t)((y * nf) + c.field) * vi.width],&c.mask[(size_t)y * lw],(size_t)lw);
        of.pixels += c.ds.pixels;
        of.temporal += fs.temporal;
        of.spatial += fs.spatial;
        of.threshold = std::max(of.threshold,c.ds.threshold);
        of.noise = std::max(of.noise,c.ds.noise);
        of.too_much += c.ds.too_much ? 1u : 0u;
        total_pixels += c.ds.pixels;
        total_temporal += fs.temporal;
        total_spatial += fs.spatial;

        // how it moved to the pictures around it is not needed again
        c.mv.clear();

        if (--of.pictures_left == 0u) {
            bool ok = on_frame(of);
            av_frame_free(&of.work);
            frames.erase(it);
            return ok;
        }

        return true;
    }

    const options_t&            opt;
    const video_info_t&         vi;
    int                         near = 2;
    detect_params_t             dp;
    me_params_t                 mp;
    std::deque<std::unique_ptr<pic_t>> pics;
    std::map<int64_t,out_frame_t> frames;
    std::vector<plane16_t>      held[2];
    int64_t                     next_seq = 0;
    int64_t                     next_frame = 0;
    int64_t                     next_detect = 0;
    int64_t                     next_fill = 0;
};

/////////////////////////////////////////////////////////////////////////////
// the output file

static bool open_output(const options_t &opt,const video_info_t &vi,input_t &in,output_t &out,enum AVPixelFormat &enc_fmt) {
    const AVStream *ist = in.vstream();
    const AVCodecParameters *ipar = ist->codecpar;
    const bool alpha = opt.mode == MODE_ALPHA && !opt.show;
    int r;

    if ((r=avformat_alloc_output_context2(&out.oc,NULL,NULL,opt.output.c_str())) < 0 || out.oc == NULL) {
        if ((r=avformat_alloc_output_context2(&out.oc,NULL,"mov",opt.output.c_str())) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
    }
    if ((out.pkt=av_packet_alloc()) == NULL)
        return false;
    if (alpha && strcmp(out.oc->oformat->name,"mov") != 0)
        fprintf(stderr,"%s: the holes of -mode 2 are meant for a QuickTime .mov file, not %s\n",opt.output.c_str(),out.oc->oformat->name);

    // the video encoder: for holes, one that keeps an alpha channel
    const AVCodec *codec;
    if (alpha && opt.codec.empty())
        codec = avcodec_find_encoder_by_name("prores_ks");
    else
        codec = choose_encoder(opt,ipar->codec_id);
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

    // the same pixel format as the input, if the encoder can take it, or with an alpha channel for holes
    const enum AVPixelFormat *fmts = encoder_pix_fmts(codec);
    enc_fmt = vi.dec_fmt;
    if (fmts != NULL) {
        bool found = false;
        for (const enum AVPixelFormat *p=fmts;*p != AV_PIX_FMT_NONE;p++) {
            if (*p == vi.dec_fmt && (!alpha || pixfmt_has_alpha(*p))) found = true;
        }
        if (!found)
            enc_fmt = avcodec_find_best_pix_fmt_of_list(fmts,vi.dec_fmt,(alpha || pixfmt_has_alpha(vi.dec_fmt)) ? 1 : 0,NULL);
        if (alpha && found == false && fmts != NULL) {
            // the least loss of the formats with alpha
            std::vector<enum AVPixelFormat> af;
            for (const enum AVPixelFormat *p=fmts;*p != AV_PIX_FMT_NONE;p++) {
                if (pixfmt_has_alpha(*p) && pixfmt_workable(*p)) af.push_back(*p);
            }
            af.push_back(AV_PIX_FMT_NONE);
            enc_fmt = avcodec_find_best_pix_fmt_of_list(&af[0],vi.dec_fmt,1,NULL);
        }
    }
    else if (alpha) {
        enc_fmt = AV_PIX_FMT_NONE;
    }
    if (alpha && (enc_fmt == AV_PIX_FMT_NONE || !pixfmt_has_alpha(enc_fmt) || !pixfmt_workable(enc_fmt))) {
        fprintf(stderr,"The %s encoder cannot keep an alpha channel, which -mode 2 needs: use prores_ks\n",codec->name);
        return false;
    }
    if (enc_fmt == AV_PIX_FMT_NONE) {
        fprintf(stderr,"The %s encoder takes no pixel format this can give it\n",codec->name);
        return false;
    }

    AVCodecContext *enc = out.enc = avcodec_alloc_context3(codec);
    if (enc == NULL)
        return false;

    const AVRational fr = vi.frame_rate;
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

    if (alpha && codec->id == AV_CODEC_ID_PRORES) {
        // ProRes 4444, the kind of ProRes that has alpha
        av_opt_set(enc->priv_data,"profile","4444",0);
    }
    else if (codec->id == ipar->codec_id) {
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
    const bool has_timecode = av_dict_get(ist->metadata,"timecode",NULL,0) != NULL && opt.start_us <= 0;
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
            // which is not the timecode of the frame -ss starts at
            if (opt.start_us > 0)
                av_dict_set(&os->metadata,"timecode",NULL,0);
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

// the alpha of each pixel of a frame, from its dropouts: clear in them, opaque away from them,
// and if feather, fading from one to the other over that many pixels outside them
static void make_alpha(std::vector<float> &a,const mask_t &mask,int w,int h,double feather) {
    const size_t n = (size_t)w * h;

    a.assign(n,1.0f);
    if (feather <= 0) {
        for (size_t i=0;i < n;i++) {
            if (mask[i]) a[i] = 0.0f;
        }
        return;
    }

    // the dropouts grown by the feather, then blurred by it, which is all 1 in the dropouts
    const int r = std::max(1,(int)ceil(feather));
    mask_t grown = mask;
    std::vector<float> b(n),t(n);

    grow_mask(grown,w,h,r,r);
    for (size_t i=0;i < n;i++)
        b[i] = grown[i] ? 1.0f : 0.0f;
    for (int y=0;y < h;y++) {
        for (int x=0;x < w;x++) {
            float s = 0;
            for (int k=-r;k <= r;k++)
                s += b[((size_t)y * w) + std::min(std::max(x + k,0),w - 1)];
            t[((size_t)y * w) + x] = s / (float)((r * 2) + 1);
        }
    }
    for (int y=0;y < h;y++) {
        for (int x=0;x < w;x++) {
            float s = 0;
            for (int k=-r;k <= r;k++)
                s += t[((size_t)std::min(std::max(y + k,0),h - 1) * w) + x];
            const size_t i = ((size_t)y * w) + x;
            a[i] = mask[i] ? 0.0f : std::max(0.0f,1.0f - (s / (float)((r * 2) + 1)));
        }
    }
}

// the alpha channel of a frame of a workable format with alpha
static void set_alpha(AVFrame *f,const std::vector<float> &a) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get((enum AVPixelFormat)f->format);
    const AVComponentDescriptor &c = d->comp[d->nb_components - 1u];
    const float maxv = (float)((1u << c.depth) - 1u);

    for (int y=0;y < f->height;y++) {
        uint8_t *row = f->data[c.plane] + ((ptrdiff_t)y * f->linesize[c.plane]);
        const float *s = &a[(size_t)y * f->width];
        if (c.depth > 8) {
            uint16_t *r16 = (uint16_t*)row;
            for (int x=0;x < f->width;x++)
                r16[x] = (uint16_t)((s[x] * maxv) + 0.5f);
        }
        else {
            for (int x=0;x < f->width;x++)
                row[x] = (uint8_t)((s[x] * maxv) + 0.5f);
        }
    }
}

static bool write_log_header(FILE *fp) {
    return fprintf(fp,"frame,time,dropout_pixels,dropout_percent,threshold,noise,flash,filled_from_other_pictures,filled_from_lines\n") > 0;
}

static bool run(const options_t &opt) {
    input_t in;
    output_t out;
    video_info_t vi;
    enum AVPixelFormat enc_fmt = AV_PIX_FMT_NONE;
    converter_t conv_in,conv_out;
    std::unique_ptr<pipeline_t> pipe;
    std::vector<AVPacket*> early;                   // packets of the other streams before the output is open
    FILE *logfp = NULL;
    bool opened = false;
    bool ok_write_early = true;
    unsigned long written = 0;

    in.start_us = opt.start_us;
    in.length_us = opt.length_us;
    in.stop_message = "CTRL+C: stopping here, and finishing the output with what there is (CTRL+C again quits at once)";
    if (!in.open(opt.input,false))
        return false;

    const AVStream *st = in.vstream();
    const AVRational itb = st->time_base;
    const int64_t offset_us = in.offset_us();       // taken off the time of everything written, for -ss

    if (!opt.log_path.empty()) {
        if ((logfp=fopen(opt.log_path.c_str(),"w")) == NULL) {
            fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
            return false;
        }
        write_log_header(logfp);
    }

    vi.frame_rate = av_guess_frame_rate(in.fmt,in.vstream(),NULL);
    progress_t progress("Filtering",frame_count(in),opt.quiet);

    auto write_packet = [&](AVPacket *pkt) -> bool {
        const int o = out.map[(size_t)pkt->stream_index];
        if (o < 0)
            return true;

        const AVRational tb = in.fmt->streams[pkt->stream_index]->time_base;
        if (offset_us != 0) {
            const int64_t off = av_rescale_q(offset_us,av_make_q(1,AV_TIME_BASE),tb);
            if (pkt->pts != AV_NOPTS_VALUE) pkt->pts -= off;
            if (pkt->dts != AV_NOPTS_VALUE) pkt->dts -= off;
        }
        av_packet_rescale_ts(pkt,tb,out.oc->streams[o]->time_base);
        pkt->stream_index = o;
        pkt->pos = -1;

        int r = av_interleaved_write_frame(out.oc,pkt);
        if (r < 0) {
            fprintf(stderr,"\nError writing audio: %s\n",averr(r).c_str());
            return false;
        }
        return true;
    };

    // encode a frame the pipeline is done with
    auto encode = [&](out_frame_t &of) -> bool {
        AVFrame *ef = of.work;
        AVFrame *conv = NULL;

        if (vi.work_fmt != enc_fmt) {
            if ((conv=alloc_frame(enc_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (!conv_out.convert(conv,of.work)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name(vi.work_fmt),av_get_pix_fmt_name(enc_fmt));
                av_frame_free(&conv);
                return false;
            }
            av_frame_copy_props(conv,of.work);
            ef = conv;
        }
        else {
            // the encoder may hold on to it, and the pipeline frees its own
            ef = av_frame_clone(of.work);
            if (ef == NULL)
                return false;
        }

        if (opt.mode == MODE_ALPHA && !opt.show) {
            std::vector<float> a;
            if (av_frame_make_writable(ef) < 0) {
                av_frame_free(&ef);
                return false;
            }
            make_alpha(a,of.mask,vi.width,vi.height,opt.feather);
            set_alpha(ef,a);
        }

        ef->pict_type = AV_PICTURE_TYPE_NONE;
        ef->sample_aspect_ratio = vi.sar;
        if (out.enc->color_range == AVCOL_RANGE_JPEG)
            ef->color_range = AVCOL_RANGE_JPEG;
        if (field_order_interlaced(vi.field_order) && !frame_is_interlaced(ef))
            frame_set_interlaced(ef,vi.tff);

        // timestamps in the encoder's time base, always going forward
        int64_t pts = of.pts;
        if (pts != AV_NOPTS_VALUE) {
            pts -= av_rescale_q(offset_us,av_make_q(1,AV_TIME_BASE),itb);
            pts = av_rescale_q(pts,itb,out.enc->time_base);
        }
        else {
            pts = (out.last_pts != AV_NOPTS_VALUE) ? (out.last_pts + std::max((int64_t)1,out.frame_duration)) : 0;
        }
        if (out.last_pts != AV_NOPTS_VALUE && pts <= out.last_pts)
            pts = out.last_pts + 1;
        ef->pts = out.last_pts = pts;

        if (logfp != NULL) {
            const double t = of.pts != AV_NOPTS_VALUE ? (double)in.time_us(of.pts,itb) / AV_TIME_BASE : NAN;
            fprintf(logfp,"%lld,%.4f,%zu,%.4f,%.2f,%.2f,%u,%zu,%zu\n",(long long)of.index,t,of.pixels,
                (100.0 * (double)of.pixels) / ((double)vi.width * (double)vi.height),of.threshold,of.noise,of.too_much,of.temporal,of.spatial);
        }

        int r = avcodec_send_frame(out.enc,ef);
        av_frame_free(&ef);
        if (r < 0) {
            fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
            return false;
        }
        if (!out.write_encoded())
            return false;

        progress.frame(++written);
        return true;
    };

    bool ok = in.run([&](AVFrame *frame) -> bool {
        const int64_t pts = frame->best_effort_timestamp;

        if (!opened) {
            // what the video is like, from its first frame
            vi.width = frame->width;
            vi.height = frame->height;
            vi.dec_fmt = (enum AVPixelFormat)frame->format;
            vi.work_fmt = work_format_for(vi.dec_fmt);
            vi.sar = frame->sample_aspect_ratio.num > 0 ? frame->sample_aspect_ratio : st->sample_aspect_ratio;
            vi.color_range = frame->color_range;
            vi.color_primaries = frame->color_primaries;
            vi.color_trc = frame->color_trc;
            vi.colorspace = frame->colorspace;
            vi.chroma_location = frame->chroma_location;
            vi.rgb = !!(av_pix_fmt_desc_get(vi.work_fmt)->flags & AV_PIX_FMT_FLAG_RGB);

            vi.field_order = st->codecpar->field_order;
            if (!field_order_interlaced(vi.field_order) && frame_is_interlaced(frame))
                vi.field_order = frame_is_tff(frame) ? AV_FIELD_TT : AV_FIELD_BB;
            vi.fields = opt.fields >= 0 ? (opt.fields == 1) : (vi.field_order != AV_FIELD_PROGRESSIVE);
            vi.tff = !(vi.field_order == AV_FIELD_BB || vi.field_order == AV_FIELD_BT);
            if (vi.fields && vi.height < 16) {
                fprintf(stderr,"%s: too few lines to take the fields apart\n",opt.input.c_str());
                return false;
            }

            if (!opt.quiet) {
                fprintf(stderr,"%s: %dx%d %s, %s, %s\n",opt.input.c_str(),vi.width,vi.height,av_get_pix_fmt_name(vi.dec_fmt),
                    field_order_name(vi.field_order),vi.fields ? "each field a picture of its own" : "each frame a picture");
                fprintf(stderr,"-mode %d: %s\n",opt.mode,opt.show ? "marking the dropouts in magenta" :
                    opt.mode == MODE_HOLD ? "filling in with what was last there" :
                    opt.mode == MODE_ALPHA ? "making holes of the dropouts" : "filling in from the pictures around them");
            }

            if (!open_output(opt,vi,in,out,enc_fmt))
                return false;
            opened = true;

            for (AVPacket *p : early) {
                if (ok_write_early && !write_packet(p))
                    ok_write_early = false;
                av_packet_free(&p);
            }
            early.clear();
            if (!ok_write_early)
                return false;

            pipe.reset(new pipeline_t(opt,vi));
            pipe->on_frame = encode;
        }

        if (frame->width != vi.width || frame->height != vi.height) {
            fprintf(stderr,"\n%s: the frame size changes, which this cannot do\n",opt.input.c_str());
            return false;
        }

        AVFrame *work = alloc_frame(vi.work_fmt,vi.width,vi.height);
        if (work == NULL)
            return false;
        if (frame->format != vi.work_fmt) {
            if (!conv_in.convert(work,frame)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format),av_get_pix_fmt_name(vi.work_fmt));
                av_frame_free(&work);
                return false;
            }
        }
        else {
            av_frame_copy(work,frame);
        }
        av_frame_copy_props(work,frame);
        work->color_range = frame->color_range;
        work->colorspace = frame->colorspace;

        return pipe->add(work,pts);
    },[&](AVPacket *pkt) -> bool {
        // the other streams, as they are, for the part of the video being done
        if (!opened) {
            AVPacket *c = av_packet_clone(pkt);
            if (c == NULL)
                return false;
            early.push_back(c);
            return true;
        }
        return write_packet(pkt);
    });

    for (AVPacket *p : early)
        av_packet_free(&p);

    if (!opened) {
        if (in.stopped)
            fprintf(stderr,"Stopped by CTRL+C before any video, nothing written\n");
        else if (ok)
            fprintf(stderr,"%s: no video %s\n",opt.input.c_str(),opt.start_us > 0 ? "after the start" : "in it");
        if (logfp != NULL) fclose(logfp);
        return false;
    }

    if (ok)
        ok = pipe->finish();
    progress.done(written);

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
    if (logfp != NULL && fclose(logfp) != 0) {
        fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
        ok = false;
    }
    if (!ok)
        fprintf(stderr,"%s is not complete\n",opt.output.c_str());
    else if (in.stopped)
        fprintf(stderr,"Stopped by CTRL+C: %s has the %lu frames before it, and is complete\n",opt.output.c_str(),written);

    if (!opt.quiet) {
        const unsigned int nf = vi.fields ? 2u : 1u;
        const double all = (double)written * nf;
        fprintf(stderr,"Dropouts in %lu of %.0f %s (%.1f%%), %.3f%% of the picture",pipe->with_dropouts,all,vi.fields ? "fields" : "frames",
            all > 0 ? (100.0 * (double)pipe->with_dropouts) / all : 0.0,
            written > 0 ? (100.0 * (double)pipe->total_pixels) / ((double)written * vi.width * vi.height) : 0.0);
        if (opt.mode != MODE_ALPHA && !opt.show && (pipe->total_temporal + pipe->total_spatial) != 0u) {
            fprintf(stderr,", %.1f%% of it filled in from %s, the rest from the lines around it",
                (100.0 * (double)pipe->total_temporal) / (double)(pipe->total_temporal + pipe->total_spatial),
                opt.mode == MODE_HOLD ? "what was held" : "the pictures around it");
        }
        fprintf(stderr,"\n");
        if (pipe->flashes != 0u)
            fprintf(stderr,"%lu %s differed too much from the pictures around to be dropouts (-area)\n",pipe->flashes,vi.fields ? "fields" : "frames");
        if (pipe->cuts != 0u)
            fprintf(stderr,"%lu %s filled in from the lines around the dropouts, at cuts (-cut)\n",pipe->cuts,vi.fields ? "fields were" : "frames were");
    }

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
    int grow[2] = {-1,-1};

    for (int i=1;i < argc;i++) {
        const char *a = argv[i];

        if (a[0] == '-' && a[1] != 0) {
            const std::string o = a + 1;
            // options that take a value
            const bool takes = (o == "mode" || o == "level" || o == "noise" || o == "local" || o == "strong" || o == "run" || o == "grow" ||
                                o == "near" || o == "area" || o == "margin" || o == "cut" || o == "feather" ||
                                o == "c" || o == "b" || o == "x" || o == "ss" || o == "t" || o == "log" ||
                                o == "threads");
            const char *v = NULL;
            double d;

            if (takes) {
                if (++i >= argc) {
                    fprintf(stderr,"%s needs a value\n",a);
                    return 1;
                }
                v = argv[i];
            }

            if (o == "mode") {
                if (!parse_double(v,d) || (d != 1 && d != 2 && d != 3)) {
                    fprintf(stderr,"-mode %s: must be 1, 2 or 3\n",v);
                    return 1;
                }
                opt.mode = (int)d;
            }
            else if (o == "level") {
                if (!parse_double(v,d) || d <= 0 || d > 255) {
                    fprintf(stderr,"-level %s: must be more than 0, up to 255 levels\n",v);
                    return 1;
                }
                opt.dp.threshold = (float)d;
            }
            else if (o == "noise") {
                if (!parse_double(v,d) || d < 0 || d > 100) {
                    fprintf(stderr,"-noise %s: must be 0 to 100\n",v);
                    return 1;
                }
                opt.dp.noise = (float)d;
            }
            else if (o == "local") {
                if (!parse_double(v,d) || d < 0 || d > 100) {
                    fprintf(stderr,"-local %s: must be 0 to 100\n",v);
                    return 1;
                }
                opt.dp.local = (float)d;
            }
            else if (o == "strong") {
                if (!parse_double(v,d) || d < 1 || d > 100) {
                    fprintf(stderr,"-strong %s: must be 1 to 100\n",v);
                    return 1;
                }
                opt.dp.strong = (float)d;
            }
            else if (o == "run") {
                if (!parse_double(v,d) || d < 1 || d > 10000 || d != floor(d)) {
                    fprintf(stderr,"-run %s: must be 1 or more pixels\n",v);
                    return 1;
                }
                opt.dp.min_run = (int)d;
            }
            else if (o == "grow") {
                char extra;
                if (sscanf(v,"%d,%d%c",&grow[0],&grow[1],&extra) != 2 || grow[0] < 0 || grow[1] < 0 || grow[0] > 100 || grow[1] > 100) {
                    fprintf(stderr,"-grow %s: give the pixels and lines, such as 3,2\n",v);
                    return 1;
                }
            }
            else if (o == "near") {
                if (!parse_double(v,d) || d < 1 || d > 4 || d != floor(d)) {
                    fprintf(stderr,"-near %s: must be 1 to 4 pictures\n",v);
                    return 1;
                }
                opt.near = (int)d;
            }
            else if (o == "area") {
                if (!parse_double(v,d) || d <= 0 || d > 1) {
                    fprintf(stderr,"-area %s: must be more than 0, up to 1\n",v);
                    return 1;
                }
                opt.dp.max_area = (float)d;
            }
            else if (o == "margin") {
                int m[4];
                char extra;
                if (sscanf(v,"%d,%d,%d,%d%c",&m[0],&m[1],&m[2],&m[3],&extra) != 4 || m[0] < 0 || m[1] < 0 || m[2] < 0 || m[3] < 0) {
                    fprintf(stderr,"-margin %s: give the top, bottom, left and right margins, such as 2,10,0,0\n",v);
                    return 1;
                }
                for (int j=0;j < 4;j++) opt.margin[j] = m[j];
            }
            else if (o == "cut") {
                if (!parse_double(v,d) || d <= 0 || d > 255) {
                    fprintf(stderr,"-cut %s: must be more than 0, up to 255 levels\n",v);
                    return 1;
                }
                opt.cut = d;
            }
            else if (o == "feather") {
                if (!parse_double(v,d) || d < 0 || d > 100) {
                    fprintf(stderr,"-feather %s: must be 0 to 100 pixels\n",v);
                    return 1;
                }
                opt.feather = d;
            }
            else if (o == "nomatch") {
                opt.fp.match = false;
            }
            else if (o == "show") {
                opt.show = true;
            }
            else if (o == "ss") {
                if (!parse_time(v,opt.start_us)) {
                    fprintf(stderr,"-ss %s: give the time as seconds, such as 90.5, or [HH:]MM:SS[.m...], such as 1:30.5\n",v);
                    return 1;
                }
            }
            else if (o == "t") {
                if (!parse_time(v,opt.length_us) || opt.length_us == 0) {
                    fprintf(stderr,"-t %s: give the time as seconds, such as 90.5, or [HH:]MM:SS[.m...], such as 1:30.5\n",v);
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

    if (files.size() != 2u) {
        help();
        return 1;
    }
    if (opt.mode == 0) {
        fprintf(stderr,"-mode is needed: 1 to fill in with what was last there, 2 to make holes, 3 to fill in from the pictures around\n");
        return 1;
    }
    opt.input = files[0];
    opt.output = files[1];
    if (same_file(opt.input,opt.output)) {
        fprintf(stderr,"%s: the output would write over the input\n",opt.output.c_str());
        return 1;
    }
    if (!opt.overwrite && file_exists(opt.output)) {
        fprintf(stderr,"%s already exists (-y writes over it)\n",opt.output.c_str());
        return 1;
    }

    // the margins and how much a dropout grows are in pixels and lines of the frame, worked out for
    // the size of the video when it is known; -1 is the default
    opt.dp.margin[0] = opt.margin[0];
    opt.dp.margin[1] = opt.margin[1];
    opt.dp.margin[2] = opt.margin[2] >= 0 ? opt.margin[2] : 0;
    opt.dp.margin[3] = opt.margin[3] >= 0 ? opt.margin[3] : 0;
    opt.dp.grow_x = grow[0] >= 0 ? grow[0] : 3;
    opt.dp.grow_y = grow[1];

    av_log_set_level(opt.quiet ? AV_LOG_ERROR : AV_LOG_WARNING);

    // OpenMP threads wait for work by spinning, which takes the CPUs from the decoder and encoder
    // if there are as many of them as CPUs
    if (opt.threads > 0)
        omp_set_num_threads(opt.threads);
    else if (getenv("OMP_NUM_THREADS") == NULL)
        omp_set_num_threads(std::max(1,omp_get_num_procs() / 2));

    catch_interrupt();

    return run(opt) ? 0 : 1;
}
