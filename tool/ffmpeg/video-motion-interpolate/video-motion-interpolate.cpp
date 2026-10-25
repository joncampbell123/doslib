/* video-motion-interpolate: make video of a low frame rate, such as an old capture card's AVI at 15 frames
 * a second, into video of a higher one, each new frame made by moving the pixels of the frames either side
 * of it along the paths they move on.
 *
 * The video is read twice. First, at its own size, for how the picture moves from each frame to the next,
 * to find the frames the capture missed, which it made up for by repeating the frame before, or not at all:
 * a frame the same as the one before it, in the middle of things moving, is a repeat, and is left out; and
 * where the picture moves twice (or three times) as far from one frame to the next as from the frames
 * before and after, frames were missed there. Each frame is then put at the time it was really captured,
 * from where the frames were missed, and the time between all of the frames made as long as it was, so
 * that the video stays with the sound. And where nothing matches from one frame to the next, there is a
 * cut, which is not interpolated across.
 *
 * Then each new frame is made from the two frames it goes between, a and b, and how the picture moves
 * from a to b and from b to a, and from the frame before a to a and from b to the frame after it, which
 * gives each pixel's path a curve, as it speeds up, slows down or turns (-near 2). Each pixel of the new
 * frame is followed back along its path to a and on to b, both ways, and where a thing is seen in only one
 * of them, covered or uncovered by something moving in front of it, it is taken from that one alone.
 *
 * -upscale makes the frames bigger first (the motion is still measured at their own size): moving a pixel
 * part of the way to the next takes a little from the pixels around it, which smudges a small picture,
 * and much less of a bigger one.
 *
 * The audio is copied as it is. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
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

#include "flow.h"
#include "interp.h"

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 codec;              // video encoder name
    std::string                 codec_opts;         // key=value:key=value
    std::string                 log_path;
    AVRational                  rate = {60000,1001};    // frames a second of the output
    double                      upscale = 1;        // how many times as big the output is
    int                         near = 2;           // frames each side of a new frame whose motion is used: 1 or 2
    bool                        drops = true;       // find the frames the capture missed or repeated
    int64_t                     bit_rate = 0;
    int64_t                     start_us = 0;       // -ss: how far into the input to start, in microseconds
    int64_t                     length_us = -1;     // -t: how much of it to do, -1 for all of it
    int                         threads = 0;        // to work on the pictures with, 0 for the CPUs
    flow_params_t               fp;
    bool                        overwrite = false;
    bool                        quiet = false;
};

static void help(void) {
    fprintf(stderr,"video-motion-interpolate [options] <input> <output>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Makes video of a low frame rate into video of a higher one, each new frame made by moving\n");
    fprintf(stderr,"the pixels of the frames either side of it along the paths they move on. The video is\n");
    fprintf(stderr,"encoded again, the audio is copied as it is.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -r <rate>      Frames a second of the output, such as 60000/1001, 59.94, 50 or 30\n");
    fprintf(stderr,"                 (default 60000/1001)\n");
    fprintf(stderr,"  -upscale <n>   Make the video this many times as big, such as 2 or 3, as part of moving\n");
    fprintf(stderr,"                 the pixels, which smudges the picture less than moving them at its own size\n");
    fprintf(stderr,"                 (default 1)\n");
    fprintf(stderr,"  -near <n>      Frames each side of a new frame whose motion is used: 1, only the two\n");
    fprintf(stderr,"                 frames it goes between, each pixel moving in a straight line; 2, and the frame\n");
    fprintf(stderr,"                 before and the frame after, for paths that speed up, slow down and turn, and\n");
    fprintf(stderr,"                 to start the motion from (default 2)\n");
    fprintf(stderr,"  -nodrops       Take each frame as being when the file says, not looking for frames the\n");
    fprintf(stderr,"                 capture missed or repeated\n");
    fprintf(stderr,"  -block <n>     Pixels across and down of the blocks the motion is measured in (default 8)\n");
    fprintf(stderr,"  -range <n>     Pixels the picture can move, at most, from one frame to the next (default 64)\n");
    fprintf(stderr,"  -log <file>    Write a CSV file of each frame of the input: when it was captured, and\n");
    fprintf(stderr,"                 whether it is a repeat, follows missed frames, or follows a cut\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Output:\n");
    fprintf(stderr,"  -c <encoder>   Video encoder, such as ffv1, prores_ks or libx264 (default: ffv1 for .mkv,\n");
    fprintf(stderr,"                 .avi and .nut, prores_ks for .mov, libx264 -crf 14 for .mp4, otherwise the\n");
    fprintf(stderr,"                 input's codec)\n");
    fprintf(stderr,"  -b <rate>      Video bit rate, such as 50M\n");
    fprintf(stderr,"  -x <k=v:k=v>   Video encoder options, such as -x crf=18\n");
    fprintf(stderr,"  -ss <time>     Start this far into the input, as FFmpeg's -ss: seconds, such as 90.5,\n");
    fprintf(stderr,"                 or [HH:]MM:SS[.m...], such as 1:30.5. The output starts at 0.\n");
    fprintf(stderr,"  -t <time>      Do only this much of the input, as FFmpeg's -t\n");
    fprintf(stderr,"  -y             Write over the output file if it exists\n");
    fprintf(stderr,"  -threads <n>   Threads to work on the pictures with (default: the CPUs)\n");
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

// the old full range YUV formats
static bool pixfmt_is_yuvj(enum AVPixelFormat fmt) {
    const char *n = av_get_pix_fmt_name(fmt);
    return n != NULL && strncmp(n,"yuvj",4) == 0;
}

static bool pixfmt_has_alpha(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    return d != NULL && (d->flags & AV_PIX_FMT_FLAG_ALPHA);
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

static bool field_order_interlaced(enum AVFieldOrder fo) {
    return fo == AV_FIELD_TT || fo == AV_FIELD_BB || fo == AV_FIELD_TB || fo == AV_FIELD_BT;
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
    bool                        export_qp = false;  // the decoder gives the quantizer of each block with each frame,
                                                    // for the post-processing filters that use it
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
        if (export_qp)
            dec->export_side_data |= AV_CODEC_EXPORT_DATA_VIDEO_ENC_PARAMS;
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
// writing the output file

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

// The video encoder: -c, or else one for the kind of file the output is, as the input was most likely a
// capture card's MPEG, too little to encode again as it was: lossless FFV1 in Matroska, AVI and NUT, ProRes
// in QuickTime, H.264 in MP4. Otherwise the input's codec.
static const AVCodec *choose_encoder(const options_t &opt,enum AVCodecID id,const AVOutputFormat *ofmt) {
    const char *name = ofmt->name;
    const char *pick = NULL;
    const AVCodec *c;

    if (!opt.codec.empty())
        return avcodec_find_encoder_by_name(opt.codec.c_str());

    if (strstr(name,"matroska") != NULL || strcmp(name,"avi") == 0 || strcmp(name,"nut") == 0)
        pick = "ffv1";
    else if (strcmp(name,"mov") == 0)
        pick = "prores_ks";
    else if (strstr(name,"mp4") != NULL)
        pick = "libx264";
    if (pick != NULL && (c=avcodec_find_encoder_by_name(pick)) != NULL)
        return c;

    // prores_ks makes better ProRes than the default ProRes encoder
    if (id == AV_CODEC_ID_PRORES && (c=avcodec_find_encoder_by_name("prores_ks")) != NULL)
        return c;
    return avcodec_find_encoder(id);
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



/////////////////////////////////////////////////////////////////////////////
// the first pass: when each frame was really captured

// what is known of each frame of the input, and of the motion from the frame before it
struct frame_info_t {
    double                      pts_time = 0;       // seconds, as the file says, from the first frame
    double                      time = 0;           // when it was really captured
    float                       change = 0;         // mean difference from the frame before, unmoved
    float                       motion = 0;         // median distance the blocks moved from the frame before
    float                       gx = 0,gy = 0;      // median vector of them
    float                       bad = 0;            // fraction of the blocks that match nothing
    float                       residual = 0;       // how badly the blocks match, on the whole: their mean cost
    bool                        repeat = false;     // a repeat of the frame before, for one the capture missed
    bool                        cut = false;        // nothing matches from the frame before
    int                         missed = 0;         // frames the capture missed before it
};

// A repeat: next to nothing changes from the frame before, where things moved before and move after.
static const float REPEAT_CHANGE = 0.6f;            // levels
static const float MOVING_CHANGE = 2.0f;
static const int REPEAT_RUN = 3;                    // repeats one after another, at most
// A cut: this much of the picture matches nothing in the frame before; or it changes this much, and the
// blocks match, on the whole, this badly, and this many times as badly as the frames around do. (A fade, or
// the light changing, changes it as much, but the blocks still match, brighter or darker.)
static const float CUT_BAD = 0.5f;
static const float CUT_CHANGE = 20.0f;
static const float CUT_RESIDUAL = 9.0f;
static const float CUT_RATIO = 1.3f;
static const float BAD_COST = 20.0f;                // levels, a block that matches this badly matches nothing
static const float GOOD_COST = 14.0f;
// Missed frames: the picture moves this many times as far as from the frames before and after, which move
// at least MISS_MOTION pixels, and about the same as each other, the same way.
static const float MISS_RATIO = 1.75f;
static const float MISS_MOTION = 1.5f;
static const float MISS_STEADY = 1.6f;
static const int MISS_MOST = 3;

// a frame of the input as 8-bit gray at its own size, into a float image
static bool frame_luma(fimage_t &l,const AVFrame *f,struct SwsContext *&sws) {
    sws = sws_getCachedContext(sws,f->width,f->height,(enum AVPixelFormat)f->format,f->width,f->height,AV_PIX_FMT_GRAY8,SWS_BILINEAR,NULL,NULL,NULL);
    if (sws == NULL)
        return false;
    std::vector<uint8_t> g((size_t)f->width * (size_t)f->height);
    uint8_t *dst[4] = { &g[0],NULL,NULL,NULL };
    int dls[4] = { f->width,0,0,0 };
    sws_scale(sws,f->data,f->linesize,0,f->height,dst,dls);
    l.alloc(f->width,f->height);
    for (size_t i=0;i < g.size();i++)
        l.v[i] = (float)g[i];
    return true;
}

static float median_of(std::vector<float> v) {
    if (v.empty())
        return 0;
    std::nth_element(v.begin(),v.begin() + (ptrdiff_t)(v.size() / 2u),v.end());
    return v[v.size() / 2u];
}

static bool analyze(const options_t &opt,std::vector<frame_info_t> &info) {
    input_t in;
    struct SwsContext *sws = NULL;
    fimage_t prev_l;
    pyr_t prev_p;
    flow_params_t fp = opt.fp;
    double t0 = NAN;

    in.start_us = opt.start_us;
    in.length_us = opt.length_us;
    in.stop_message = "CTRL+C: stopping here (CTRL+C again quits at once)";
    if (!in.open(opt.input,true))
        return false;
    const AVRational tb = in.vstream()->time_base;
    const AVRational fr = av_guess_frame_rate(in.fmt,in.vstream(),NULL);
    const double step = fr.num > 0 ? av_q2d(av_inv_q(fr)) : 1.0 / 15.0;
    progress_t progress("Looking at the motion",frame_count(in),opt.quiet);

    // coarse blocks, whole pixels: only how far, and which way, the picture moves
    fp.block = 16;
    fp.subpel = false;

    info.clear();
    bool ok = in.run([&](AVFrame *f) -> bool {
        frame_info_t fi;
        fimage_t l;
        pyr_t p;

        if (!frame_luma(l,f,sws)) {
            fprintf(stderr,"\nCannot convert %s pixels to gray\n",av_get_pix_fmt_name((enum AVPixelFormat)f->format));
            return false;
        }
        const double t = f->best_effort_timestamp != AV_NOPTS_VALUE ? (double)in.time_us(f->best_effort_timestamp,tb) / AV_TIME_BASE : NAN;
        if (std::isnan(t0))
            t0 = std::isnan(t) ? 0.0 : t;
        fi.pts_time = !std::isnan(t) ? t - t0 : (info.empty() ? 0.0 : info.back().pts_time + step);
        if (!info.empty() && fi.pts_time <= info.back().pts_time)
            fi.pts_time = info.back().pts_time + (step * 0.01);

        build_pyr(p,l);
        if (!info.empty() && l.w == prev_l.w && l.h == prev_l.h) {
            double sum = 0;
            for (size_t i=0;i < l.v.size();i++)
                sum += fabsf(l.v[i] - prev_l.v[i]);
            fi.change = (float)(sum / (double)l.v.size());

            field_t fl;
            estimate_flow(fl,prev_p,p,fp,NULL);
            std::vector<float> mags,xs,ys;
            size_t bad = 0;
            double rsum = 0;
            for (size_t i=0;i < fl.vx.size();i++) {
                rsum += fl.cost[i];
                if (fl.cost[i] >= BAD_COST)
                    bad++;
                if (fl.cost[i] < GOOD_COST) {
                    mags.push_back(sqrtf((fl.vx[i] * fl.vx[i]) + (fl.vy[i] * fl.vy[i])));
                    xs.push_back(fl.vx[i]);
                    ys.push_back(fl.vy[i]);
                }
            }
            fi.motion = median_of(mags);
            fi.gx = median_of(xs);
            fi.gy = median_of(ys);
            fi.bad = fl.vx.empty() ? 0.0f : (float)bad / (float)fl.vx.size();
            fi.residual = fl.vx.empty() ? 0.0f : (float)(rsum / (double)fl.vx.size());
        }
        prev_l = std::move(l);
        prev_p = std::move(p);
        info.push_back(fi);
        progress.frame(info.size());
        return true;
    },nullptr);
    progress.done(info.size());
    sws_freeContext(sws);
    if (in.stopped)
        return false;
    if (!ok || info.empty()) {
        if (ok)
            fprintf(stderr,"%s: no video\n",opt.input.c_str());
        return false;
    }

    const size_t n = info.size();
    for (size_t i=1;i < n;i++) {
        std::vector<float> around;
        for (size_t k=(i > 5u ? i - 5u : 1u);k <= std::min(n - 1u,i + 5u);k++) {
            if (k != i)
                around.push_back(info[k].residual);
        }
        const float typical = median_of(around);
        info[i].cut = info[i].bad > CUT_BAD ||
            (info[i].change > CUT_CHANGE && info[i].residual > std::max(CUT_RESIDUAL,CUT_RATIO * typical));
    }

    if (opt.drops) {
        // repeats: a short run of frames that are the same as the one before, between frames that move
        for (size_t i=1;i < n;) {
            if (info[i].change >= REPEAT_CHANGE || info[i].cut) {
                i++;
                continue;
            }
            size_t j = i;
            while (j < n && info[j].change < REPEAT_CHANGE && !info[j].cut)
                j++;
            const bool before = info[i - 1u].change >= MOVING_CHANGE;
            const bool after = j < n && info[j].change >= MOVING_CHANGE;
            if ((int)(j - i) <= REPEAT_RUN && before && after) {
                for (size_t k=i;k < j;k++)
                    info[k].repeat = true;
            }
            i = j;
        }

        // missed frames: from frame i-1 to frame i, the picture moved twice as far as from the frames
        // before and after, which are not repeats, nor cuts, nor next to them
        auto usable = [&](size_t i) -> bool {
            return i >= 1 && i < n && !info[i].cut && !info[i].repeat && !info[i - 1u].repeat;
        };
        for (size_t i=2;i + 1u < n;i++) {
            if (!usable(i) || !usable(i - 1u) || !usable(i + 1u))
                continue;
            const frame_info_t &a = info[i - 1u],&c = info[i],&b = info[i + 1u];
            const float lo = std::min(a.motion,b.motion),hi = std::max(a.motion,b.motion);
            if (lo < MISS_MOTION || hi > MISS_STEADY * lo)
                continue;
            const float around = 0.5f * (a.motion + b.motion);
            if (c.motion < MISS_RATIO * around)
                continue;
            // the same way as the frames around it
            auto same_way = [&](const frame_info_t &o) -> bool {
                const float d = sqrtf(((c.gx * c.gx) + (c.gy * c.gy)) * ((o.gx * o.gx) + (o.gy * o.gy)));
                return d < 1.0f || ((c.gx * o.gx) + (c.gy * o.gy)) > 0.7f * d;
            };
            if (!same_way(a) || !same_way(b))
                continue;
            info[i].missed = std::min(MISS_MOST,std::max(1,(int)lrintf(c.motion / around) - 1));
        }
    }

    // when each was really captured: each time between frames as long as the frames missed in it make it,
    // and all of them made as long as they were, together
    double total = 0,longer = 0;
    for (size_t i=1;i < n;i++) {
        const double d = info[i].pts_time - info[i - 1u].pts_time;
        total += d;
        longer += d * (1.0 + info[i].missed);
    }
    const double k = longer > 0 ? total / longer : 1.0;
    info[0].time = 0;
    for (size_t i=1;i < n;i++)
        info[i].time = info[i - 1u].time + ((info[i].pts_time - info[i - 1u].pts_time) * (1.0 + info[i].missed) * k);

    if (!opt.quiet) {
        size_t reps = 0,miss = 0,missed = 0,cuts = 0;
        for (const frame_info_t &fi : info) {
            reps += fi.repeat ? 1u : 0u;
            miss += fi.missed > 0 ? 1u : 0u;
            missed += (size_t)fi.missed;
            cuts += fi.cut ? 1u : 0u;
        }
        fprintf(stderr,"%zu frames: %zu repeats left out, %zu frames missed in %zu places, %zu cuts\n",n,reps,missed,miss,cuts);
    }

    if (!opt.log_path.empty()) {
        FILE *fp_log = fopen(opt.log_path.c_str(),"w");
        if (fp_log == NULL) {
            fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
            return false;
        }
        fprintf(fp_log,"frame,file_time,time,change,motion,motion_x,motion_y,unmatched,repeat,missed_before,cut,residual\n");
        for (size_t i=0;i < n;i++) {
            const frame_info_t &fi = info[i];
            fprintf(fp_log,"%zu,%.4f,%.4f,%.2f,%.2f,%.2f,%.2f,%.3f,%d,%d,%d,%.2f\n",i,fi.pts_time,fi.time,fi.change,fi.motion,fi.gx,fi.gy,fi.bad,
                fi.repeat ? 1 : 0,fi.missed,fi.cut ? 1 : 0,fi.residual);
        }
        if (fclose(fp_log) != 0) {
            fprintf(stderr,"%s: %s\n",opt.log_path.c_str(),strerror(errno));
            return false;
        }
    }
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// the second pass: the new frames

// The work format, of planes the new frames are made in: planar RGB for RGB, else YUV 4:4:4, 8 bits. A
// palette, as of 8-bit video for Windows, is RGB, though it has one component, the index into it.
static enum AVPixelFormat planes_format(enum AVPixelFormat fmt) {
    const AVPixFmtDescriptor *d = av_pix_fmt_desc_get(fmt);
    if (d != NULL && (d->flags & (AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_PAL)))
        return AV_PIX_FMT_GBRP;
    if (d != NULL && d->nb_components == 1)
        return AV_PIX_FMT_GRAY8;
    return pixfmt_is_yuvj(fmt) ? AV_PIX_FMT_YUVJ444P : AV_PIX_FMT_YUV444P;
}

/////////////////////////////////////////////////////////////////////////////
// the output file

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
    const AVCodec *codec = choose_encoder(opt,ipar->codec_id,out.oc->oformat);
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
    // H.264 near enough to the picture not to put back the blocks that were taken out
    if (strcmp(codec->name,"libx264") == 0 && opt.bit_rate <= 0 && opt.codec_opts.find("crf") == std::string::npos)
        av_opt_set(enc->priv_data,"crf","14",0);
    // FFV1 in slices, which are encoded at once on the CPUs, a frame a CPU being too slow for 60 of them a second
    if (strcmp(codec->name,"ffv1") == 0 && opt.codec_opts.empty()) {
        av_opt_set_int(enc->priv_data,"slices",16,0);
        enc->level = 3;
    }

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


/////////////////////////////////////////////////////////////////////////////

static bool run(const options_t &opt) {
    std::vector<frame_info_t> info;
    if (!analyze(opt,info))
        return false;

    input_t in;
    output_t out;
    video_info_t vi;
    enum AVPixelFormat enc_fmt = AV_PIX_FMT_NONE;
    converter_t conv_up,conv_out;
    struct SwsContext *sws_luma = NULL;
    std::vector<AVPacket*> early;                   // packets of the other streams before the output is open
    bool opened = false;
    bool ok_write_early = true;
    unsigned long written = 0;
    long index = -1;
    AVFrame *up = NULL,*enc_frame = NULL,*work = NULL;
    int in_w = 0,in_h = 0;

    in.start_us = opt.start_us;
    in.length_us = opt.length_us;
    in.stop_message = "CTRL+C: stopping here, and finishing the output with what there is (CTRL+C again quits at once)";
    if (!in.open(opt.input,false))
        return false;

    const AVStream *st = in.vstream();
    const int64_t offset_us = in.offset_us();       // taken off the time of everything written, for -ss

    // the output: from the first frame's time to as long after the last as the frames were apart
    const double end_time = info.size() > 1u ? info.back().time + ((info.back().time - info.front().time) / (double)(info.size() - 1u)) : 1.0;
    const double out_step = av_q2d(av_inv_q(opt.rate));
    const int64_t out_frames = std::max((int64_t)1,(int64_t)llround(end_time / out_step));
    int64_t next_out = 0;
    progress_t progress("Interpolating",out_frames,opt.quiet);

    // the real frames, as many as are needed, and the motion between each one and the next
    std::deque<std::unique_ptr<frame_t> > frames;
    std::map<long,field_t> fwd,bwd;                 // by the index of the first of the two: to the next, and back from it
    std::map<long,long> next_of;                    // the index of the real frame after each

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

    // a new frame, of the planes, encoded
    auto encode = [&](const std::vector<fimage_t> &pl) -> bool {
        if (av_frame_make_writable(work) < 0)
            return false;
        const std::vector<comp_t> comps = color_comps(vi.work_fmt);
        for (size_t c=0;c < comps.size() && c < pl.size();c++) {
            for (int y=0;y < vi.height;y++) {
                uint8_t *d = work->data[comps[c].plane] + ((ptrdiff_t)y * work->linesize[comps[c].plane]);
                const float *s = pl[c].row(y);
                for (int x=0;x < vi.width;x++)
                    d[x] = (uint8_t)std::min(255,std::max(0,(int)lrintf(s[x])));
            }
        }

        AVFrame *ef;
        if (vi.work_fmt != enc_fmt) {
            if (av_frame_make_writable(enc_frame) < 0)
                return false;
            if (!conv_out.convert(enc_frame,work)) {
                fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name(vi.work_fmt),av_get_pix_fmt_name(enc_fmt));
                return false;
            }
            ef = av_frame_clone(enc_frame);
        }
        else {
            ef = av_frame_clone(work);
        }
        if (ef == NULL)
            return false;
        ef->pict_type = AV_PICTURE_TYPE_NONE;
        ef->sample_aspect_ratio = vi.sar;
        ef->pts = av_rescale_q(next_out,av_inv_q(opt.rate),out.enc->time_base);

        int r = avcodec_send_frame(out.enc,ef);
        av_frame_free(&ef);
        if (r < 0) {
            fprintf(stderr,"\nVideo encoding error: %s\n",averr(r).c_str());
            return false;
        }
        if (!out.write_encoded())
            return false;
        next_out++;
        progress.frame(++written);
        return true;
    };

    // the motion from real frame a to the real frame after it, and back, if not done already
    auto motion = [&](const frame_t &a,const frame_t &b) {
        if (fwd.count(a.index))
            return;
        // predicted from how the picture moved before a, going on as it did, for the time to b
        const field_t *pred = NULL;
        field_t scaled;
        for (const auto &f : frames) {
            if (next_of.count(f->index) && next_of[f->index] == a.index && fwd.count(f->index) && opt.near >= 2) {
                const double dtp = a.time - f->time,dt = b.time - a.time;
                if (dtp > 0) {
                    // the motion into a, as it is at a: back from a, the other way
                    scaled = bwd[f->index];
                    for (size_t i=0;i < scaled.vx.size();i++) {
                        scaled.vx[i] *= (float)(-dt / dtp);
                        scaled.vy[i] *= (float)(-dt / dtp);
                    }
                    pred = &scaled;
                }
            }
        }
        estimate_flow(fwd[a.index],a.pyr,b.pyr,opt.fp,pred);
        // and back: the forward vectors, the other way, as the prediction
        field_t back;
        back.alloc(b.luma.w,b.luma.h,opt.fp.block);
        const field_t &f = fwd[a.index];
        for (int by=0;by < back.bh;by++) {
            for (int bx=0;bx < back.bw;bx++) {
                float vx,vy;
                f.at(((float)bx + 0.5f) * (float)back.block,((float)by + 0.5f) * (float)back.block,vx,vy);
                back.vx[((size_t)by * back.bw) + bx] = -vx;
                back.vy[((size_t)by * back.bw) + bx] = -vy;
            }
        }
        estimate_flow(bwd[a.index],b.pyr,a.pyr,opt.fp,&back);
    };

    // whether there is a cut between two real frames of the input
    auto cut_between = [&](long a,long b) -> bool {
        for (long i=a + 1;i <= b && i < (long)info.size();i++) {
            if (info[(size_t)i].cut)
                return true;
        }
        return false;
    };

    // make each new frame that the frames there are now go around: all of those before the second to last
    // real frame, so that the frame after b is there; or, at the end, all of the rest
    auto render = [&](bool at_end) -> bool {
        while (next_out < out_frames) {
            const double t = (double)next_out * out_step;
            // a: the last real frame at or before t, and b the one after it
            size_t ai = 0;
            while (ai + 1u < frames.size() && frames[ai + 1u]->time <= t)
                ai++;
            const bool have_b = ai + 1u < frames.size();
            const bool have_after = ai + 2u < frames.size();
            if (!at_end && (!have_b || (opt.near >= 2 && !have_after)))
                return true;

            const frame_t &A = *frames[ai];
            std::vector<fimage_t> pl;
            if (!have_b || t <= A.time) {
                // before the first frame, at a frame, or after the last: that frame as it is
                if (!encode(A.planes))
                    return false;
                continue;
            }
            const frame_t &B = *frames[ai + 1u];
            const float frac = (float)((t - A.time) / (B.time - A.time));
            if (cut_between(A.index,B.index)) {
                // across a cut: the frame on its side
                if (!encode(frac < 0.5f ? A.planes : B.planes))
                    return false;
                continue;
            }

            motion(A,B);
            between_t bt;
            bt.a = &A;
            bt.b = &B;
            bt.ab = &fwd[A.index];
            bt.ba = &bwd[A.index];
            bt.dt = B.time - A.time;
            bt.scale = (float)vi.width / (float)in_w;
            if (opt.near >= 2) {
                if (ai >= 1u && fwd.count(frames[ai - 1u]->index) && !cut_between(frames[ai - 1u]->index,A.index)) {
                    // from a back to the frame before it
                    bt.a_prev = &bwd[frames[ai - 1u]->index];
                    bt.dt_prev = A.time - frames[ai - 1u]->time;
                }
                if (have_after && !cut_between(B.index,frames[ai + 2u]->index)) {
                    motion(B,*frames[ai + 2u]);
                    bt.b_next = &fwd[B.index];
                    bt.dt_next = frames[ai + 2u]->time - B.time;
                }
            }
            interpolate(pl,bt,frac);
            if (!encode(pl))
                return false;
        }
        return true;
    };

    bool ok = in.run([&](AVFrame *frame) -> bool {
        index++;
        if ((size_t)index >= info.size() || info[(size_t)index].repeat)
            return true;

        if (!opened) {
            in_w = frame->width;
            in_h = frame->height;
            vi.width = std::max(2,(int)lrint((double)in_w * opt.upscale)) & ~1;
            vi.height = std::max(2,(int)lrint((double)in_h * opt.upscale)) & ~1;
            vi.dec_fmt = vi.work_fmt = planes_format((enum AVPixelFormat)frame->format);
            vi.sar = frame->sample_aspect_ratio.num > 0 ? frame->sample_aspect_ratio : st->sample_aspect_ratio;
            vi.frame_rate = opt.rate;
            vi.color_range = frame->color_range;
            vi.color_primaries = frame->color_primaries;
            vi.color_trc = frame->color_trc;
            vi.colorspace = frame->colorspace;
            vi.chroma_location = frame->chroma_location;
            vi.field_order = AV_FIELD_PROGRESSIVE;
            vi.fields = false;

            if (!opt.quiet)
                fprintf(stderr,"%s: %dx%d %s, to %dx%d at %d/%d frames a second (%.3f), %s\n",opt.input.c_str(),in_w,in_h,
                    av_get_pix_fmt_name((enum AVPixelFormat)frame->format),vi.width,vi.height,opt.rate.num,opt.rate.den,av_q2d(opt.rate),
                    opt.near >= 2 ? "following the motion through the frames before and after" : "each pixel moving in a straight line");

            if (!open_output(opt,vi,in,out,enc_fmt))
                return false;
            opened = true;
            if ((up=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL || (work=alloc_frame(vi.work_fmt,vi.width,vi.height)) == NULL)
                return false;
            if (vi.work_fmt != enc_fmt && (enc_frame=alloc_frame(enc_fmt,vi.width,vi.height)) == NULL)
                return false;

            for (AVPacket *p : early) {
                if (ok_write_early && !write_packet(p))
                    ok_write_early = false;
                av_packet_free(&p);
            }
            early.clear();
            if (!ok_write_early)
                return false;
        }
        if (frame->width != in_w || frame->height != in_h) {
            fprintf(stderr,"\n%s: the frame size changes, which this cannot do\n",opt.input.c_str());
            return false;
        }

        // the frame: its luma to match with, and its planes, upscaled
        std::unique_ptr<frame_t> f(new frame_t);
        f->index = index;
        f->time = info[(size_t)index].time;
        if (!frame_luma(f->luma,frame,sws_luma))
            return false;
        build_pyr(f->pyr,f->luma);
        if (av_frame_make_writable(up) < 0 || !conv_up.convert(up,frame)) {
            fprintf(stderr,"\nCannot convert %s pixels to %s\n",av_get_pix_fmt_name((enum AVPixelFormat)frame->format),av_get_pix_fmt_name(vi.work_fmt));
            return false;
        }
        const std::vector<comp_t> comps = color_comps(vi.work_fmt);
        f->planes.resize(comps.size());
        for (size_t c=0;c < comps.size();c++) {
            f->planes[c].alloc(vi.width,vi.height);
            for (int y=0;y < vi.height;y++) {
                const uint8_t *s = up->data[comps[c].plane] + ((ptrdiff_t)y * up->linesize[comps[c].plane]);
                float *d = f->planes[c].row(y);
                for (int x=0;x < vi.width;x++)
                    d[x] = (float)s[x];
            }
        }
        if (!frames.empty())
            next_of[frames.back()->index] = index;
        frames.push_back(std::move(f));

        if (!render(false))
            return false;

        // what is not needed again: the frames before the frame before a, and their motion
        const double t = (double)next_out * out_step;
        while (frames.size() > 3u && frames[2]->time <= t) {
            const long gone = frames.front()->index;
            fwd.erase(gone);
            bwd.erase(gone);
            next_of.erase(gone);
            frames.pop_front();
        }
        return true;
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
        return false;
    }

    // the rest of the new frames, to the end
    if (ok && !in.stopped)
        ok = render(true);
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
    av_frame_free(&up);
    av_frame_free(&work);
    av_frame_free(&enc_frame);
    sws_freeContext(sws_luma);
    if (!ok)
        fprintf(stderr,"%s is not complete\n",opt.output.c_str());
    else if (in.stopped)
        fprintf(stderr,"Stopped by CTRL+C: %s has the %lu frames before it, and is complete\n",opt.output.c_str(),written);

    return ok;
}

/////////////////////////////////////////////////////////////////////////////

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
            const bool takes = (o == "r" || o == "upscale" || o == "near" || o == "block" || o == "range" || o == "log" ||
                                o == "c" || o == "b" || o == "x" || o == "ss" || o == "t" || o == "threads");
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
                AVRational q;
                if (av_parse_video_rate(&q,v) < 0 || q.num <= 0 || q.den <= 0) {
                    fprintf(stderr,"-r %s: give the frames a second, such as 60000/1001, 59.94 or 30\n",v);
                    return 1;
                }
                opt.rate = q;
            }
            else if (o == "upscale") {
                if (!parse_double(v,d) || d < 1 || d > 8) {
                    fprintf(stderr,"-upscale %s: must be 1 to 8\n",v);
                    return 1;
                }
                opt.upscale = d;
            }
            else if (o == "near") {
                if (!parse_double(v,d) || (d != 1 && d != 2)) {
                    fprintf(stderr,"-near %s: must be 1 or 2\n",v);
                    return 1;
                }
                opt.near = (int)d;
            }
            else if (o == "nodrops") {
                opt.drops = false;
            }
            else if (o == "block") {
                if (!parse_double(v,d) || d < 4 || d > 64 || d != floor(d)) {
                    fprintf(stderr,"-block %s: must be 4 to 64 pixels\n",v);
                    return 1;
                }
                opt.fp.block = (int)d;
            }
            else if (o == "range") {
                if (!parse_double(v,d) || d < 1 || d > 1024 || d != floor(d)) {
                    fprintf(stderr,"-range %s: must be 1 or more pixels\n",v);
                    return 1;
                }
                opt.fp.range = (int)d;
            }
            else if (o == "log") {
                opt.log_path = v;
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
            else if (o == "threads") {
                if (!parse_double(v,d) || d < 1 || d > 1024 || d != floor(d)) {
                    fprintf(stderr,"-threads %s: must be 1 or more\n",v);
                    return 1;
                }
                opt.threads = (int)d;
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

    av_log_set_level(opt.quiet ? AV_LOG_ERROR : AV_LOG_WARNING);
    if (opt.threads > 0)
        omp_set_num_threads(opt.threads);

    catch_interrupt();

    return run(opt) ? 0 : 1;
}
