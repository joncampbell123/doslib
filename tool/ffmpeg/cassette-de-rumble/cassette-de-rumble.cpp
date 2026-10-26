/* cassette-de-rumble: take the quiet low rumble out of a cassette tape recording.
 *
 * The input is read twice. The first time, the level of the rumble at each low frequency of each channel
 * is found over the whole recording; the second time, each channel is filtered and encoded (see derumble.h).
 * Only the frequencies below the cutoff (and less and less up to twice it) are changed. The output has the
 * same sample rate and channels as the input, and is as long, lined up with it sample for sample. */

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
#include <string>
#include <vector>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/parseutils.h>
#include <libswresample/swresample.h>
}

#include "derumble.h"

struct options_t {
    std::string                 input;
    std::string                 output;
    std::string                 codec;              // audio encoder name
    int64_t                     bit_rate = 0;
    int64_t                     start_us = 0;       // -ss: how far into the input to start, in microseconds
    int64_t                     length_us = -1;     // -t: how much of it to do, -1 for all of it
    rumble_params_t             rumble;
    bool                        report = false;     // -report: the rumble level found, each minute
    bool                        dither = false;
    bool                        overwrite = false;
    bool                        quiet = false;
};

static void help(void) {
    rumble_params_t d;

    fprintf(stderr,"cassette-de-rumble [options] <input> <output>\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"Takes the quiet low rumble out of a cassette tape recording: the motor, the tape in the shell, the\n");
    fprintf(stderr,"recorder moving in a pocket. The level of the rumble is found from the quiet parts of the recording,\n");
    fprintf(stderr,"and only that much is taken out of the low frequencies, so that the bass of voices and music stays.\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -cutoff <Hz>     The rumble is taken out below this, and less and less up to twice it. Above\n");
    fprintf(stderr,"                   that nothing is changed (default %g)\n",d.cutoff);
    fprintf(stderr,"  -highpass <Hz>   Below this all of it is rumble, turned down by the most (default %g)\n",d.highpass);
    fprintf(stderr,"  -reduce <dB>     The most the rumble is turned down, 0 for not at all (default %g)\n",d.reduce);
    fprintf(stderr,"  -over <n>        How many times the level found is taken as rumble: more takes more of it out,\n");
    fprintf(stderr,"                   and more of the quiet bass with it (default %g)\n",d.over);
    fprintf(stderr,"  -window <s>      The level of the rumble is found from this many seconds around each moment,\n");
    fprintf(stderr,"                   less to follow a rumble that changes faster (default %g)\n",d.window);
    fprintf(stderr,"  -percentile <n>  The quiet part of each window that is taken as the rumble (default %g)\n",d.percentile);
    fprintf(stderr,"  -release <s>     How slowly a frequency is turned back down after a sound (default %g)\n",d.release);
    fprintf(stderr,"  -fft <n>         Samples to a transform, a power of 2: more for finer frequencies, less for finer\n");
    fprintf(stderr,"                   time (default about 85ms, 4096 at 48000Hz)\n");
    fprintf(stderr,"  -report          Print the level of the rumble found, each minute, of each channel\n");
    fprintf(stderr,"\n");
    fprintf(stderr,"  -c <codec>       Audio encoder (default: the same PCM as the input for WAV, else the format's own)\n");
    fprintf(stderr,"  -b <rate>        Bit rate, for encoders that take one, such as 192k\n");
    fprintf(stderr,"  -dither          Dither to the output's sample format (by default it is rounded, which gives\n");
    fprintf(stderr,"                   back the input as it was wherever nothing was changed)\n");
    fprintf(stderr,"  -ss <time>       Start this far into the input\n");
    fprintf(stderr,"  -t <time>        Do only this much of it\n");
    fprintf(stderr,"  -y               Overwrite the output if it is there\n");
    fprintf(stderr,"  -q               Quiet\n");
}

static std::string averr(int r) {
    char tmp[256];
    av_strerror(r,tmp,sizeof(tmp));
    return tmp;
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

// decodes the audio, as float planar samples at the input's own rate and channels
class input_t {
public:
    AVFormatContext*            fmt = NULL;
    AVCodecContext*             dec = NULL;
    SwrContext*                 swr = NULL;
    AVPacket*                   pkt = NULL;
    AVFrame*                    frm = NULL;
    int                         aindex = -1;
    int                         channels = 0;
    int                         rate = 0;
    int64_t                     skip = 0;           // samples still to drop for -ss
    int64_t                     left = -1;          // samples still to give for -t, -1 for all of them
    int64_t                     done = 0;           // samples given
    int64_t                     total = 0;          // samples expected, for the progress, 0 if not known
    bool                        eof = false;
    bool                        stopped = false;    // by CTRL+C
    std::vector<std::vector<float>> conv;

    ~input_t() {
        close();
    }

    void close(void) {
        swr_free(&swr);
        avcodec_free_context(&dec);
        avformat_close_input(&fmt);
        av_packet_free(&pkt);
        av_frame_free(&frm);
    }

    bool open(const options_t &opt) {
        int r;

        if ((r = avformat_open_input(&fmt,opt.input.c_str(),NULL,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.input.c_str(),averr(r).c_str());
            return false;
        }
        if ((r = avformat_find_stream_info(fmt,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.input.c_str(),averr(r).c_str());
            return false;
        }

        const AVCodec *codec = NULL;
        if ((aindex = av_find_best_stream(fmt,AVMEDIA_TYPE_AUDIO,-1,-1,&codec,0)) < 0 || codec == NULL) {
            fprintf(stderr,"%s: no audio that can be decoded\n",opt.input.c_str());
            return false;
        }

        AVStream *st = fmt->streams[aindex];
        dec = avcodec_alloc_context3(codec);
        avcodec_parameters_to_context(dec,st->codecpar);
        dec->pkt_timebase = st->time_base;
        if ((r = avcodec_open2(dec,codec,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.input.c_str(),averr(r).c_str());
            return false;
        }

        channels = dec->ch_layout.nb_channels;
        rate = dec->sample_rate;
        if (channels <= 0 || rate <= 0) {
            fprintf(stderr,"%s: the audio has no channels or no sample rate\n",opt.input.c_str());
            return false;
        }

        if (swr_alloc_set_opts2(&swr,&dec->ch_layout,AV_SAMPLE_FMT_FLTP,rate,&dec->ch_layout,dec->sample_fmt,rate,0,NULL) < 0 ||
            swr_init(swr) < 0) {
            fprintf(stderr,"Cannot convert the audio to float\n");
            return false;
        }

        pkt = av_packet_alloc();
        frm = av_frame_alloc();
        conv.resize(channels);

        skip = av_rescale(opt.start_us,rate,1000000);
        left = opt.length_us >= 0 ? av_rescale(opt.length_us,rate,1000000) : -1;
        if (st->duration != AV_NOPTS_VALUE && st->duration > 0)
            total = av_rescale_q(st->duration,st->time_base,AVRational{1,rate});
        else if (fmt->duration > 0)
            total = av_rescale(fmt->duration,rate,AV_TIME_BASE);
        total = std::max((int64_t)0,total - skip);
        if (left >= 0 && (total == 0 || left < total))
            total = left;

        return true;
    }

    // the next samples, into conv[ch], with n of them, false at the end
    bool read(int &n) {
        n = 0;
        while (n == 0) {
            if (eof || left == 0)
                return false;
            if (interrupted) {
                stopped = true;
                return false;
            }

            int r = avcodec_receive_frame(dec,frm);
            if (r == AVERROR(EAGAIN)) {
                r = av_read_frame(fmt,pkt);
                if (r < 0) {
                    avcodec_send_packet(dec,NULL);
                }
                else {
                    if (pkt->stream_index == aindex)
                        avcodec_send_packet(dec,pkt);
                    av_packet_unref(pkt);
                }
                continue;
            }
            if (r < 0) {
                if (r != AVERROR_EOF)
                    fprintf(stderr,"Audio decoding error: %s\n",averr(r).c_str());
                eof = true;
                return false;
            }

            const int max = swr_get_out_samples(swr,frm->nb_samples);
            std::vector<uint8_t*> ptr(channels);
            for (int c=0;c < channels;c++) {
                conv[c].resize(std::max(max,1));
                ptr[c] = (uint8_t*)conv[c].data();
            }

            int got = swr_convert(swr,ptr.data(),max,(const uint8_t**)frm->extended_data,frm->nb_samples);
            av_frame_unref(frm);
            if (got <= 0)
                continue;

            // -ss and -t, to the sample
            int from = 0;
            if (skip > 0) {
                from = (int)std::min(skip,(int64_t)got);
                skip -= from;
            }
            got -= from;
            if (left >= 0 && got > left)
                got = (int)left;
            if (got <= 0)
                continue;
            if (from > 0) {
                for (int c=0;c < channels;c++)
                    memmove(conv[c].data(),conv[c].data() + from,sizeof(float) * got);
            }
            if (left > 0)
                left -= got;

            n = got;
            done += got;
        }

        return true;
    }
};

/////////////////////////////////////////////////////////////////////////////
// encoding and writing the output file

class output_t {
public:
    AVFormatContext*            oc = NULL;
    AVCodecContext*             enc = NULL;
    AVStream*                   st = NULL;
    SwrContext*                 swr = NULL;
    AVAudioFifo*                fifo = NULL;
    AVPacket*                   pkt = NULL;
    int                         frame_size = 0;
    int64_t                     pts = 0;
    bool                        header = false;

    ~output_t() {
        if (oc != NULL && !(oc->oformat->flags & AVFMT_NOFILE))
            avio_closep(&oc->pb);
        avformat_free_context(oc);
        avcodec_free_context(&enc);
        swr_free(&swr);
        if (fifo != NULL)
            av_audio_fifo_free(fifo);
        av_packet_free(&pkt);
    }

    // the encoder for the output: as asked for, or for WAV the same PCM as the input, or the format's own
    static const AVCodec *pick_codec(const options_t &opt,const AVOutputFormat *of,const AVCodecContext *dec) {
        if (!opt.codec.empty())
            return avcodec_find_encoder_by_name(opt.codec.c_str());

        if (of->audio_codec == AV_CODEC_ID_PCM_S16LE) {
            enum AVCodecID id = AV_CODEC_ID_PCM_S16LE;
            switch (dec->sample_fmt) {
                case AV_SAMPLE_FMT_U8:  case AV_SAMPLE_FMT_U8P:  id = AV_CODEC_ID_PCM_U8; break;
                case AV_SAMPLE_FMT_S32: case AV_SAMPLE_FMT_S32P:
                    id = (dec->bits_per_raw_sample > 0 && dec->bits_per_raw_sample <= 24) ? AV_CODEC_ID_PCM_S24LE : AV_CODEC_ID_PCM_S32LE; break;
                case AV_SAMPLE_FMT_FLT: case AV_SAMPLE_FMT_FLTP: id = AV_CODEC_ID_PCM_F32LE; break;
                case AV_SAMPLE_FMT_DBL: case AV_SAMPLE_FMT_DBLP: id = AV_CODEC_ID_PCM_F64LE; break;
                default: break;
            }
            return avcodec_find_encoder(id);
        }

        return avcodec_find_encoder(of->audio_codec);
    }

    bool open(const options_t &opt,const input_t &in) {
        int r;

        if ((r = avformat_alloc_output_context2(&oc,NULL,NULL,opt.output.c_str())) < 0 || oc == NULL) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),r < 0 ? averr(r).c_str() : "unknown format");
            return false;
        }

        const AVCodec *codec = pick_codec(opt,oc->oformat,in.dec);
        if (codec == NULL || codec->type != AVMEDIA_TYPE_AUDIO) {
            fprintf(stderr,"No audio encoder %s\n",opt.codec.empty() ? "for that format" : opt.codec.c_str());
            return false;
        }

        enc = avcodec_alloc_context3(codec);
        // a WAV file may say only how many channels: the usual layout of that many, which encoders take
        if (in.dec->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
            av_channel_layout_default(&enc->ch_layout,in.channels);
        else
            av_channel_layout_copy(&enc->ch_layout,&in.dec->ch_layout);
        enc->sample_rate = in.rate;
        enc->time_base = AVRational{1,in.rate};
        if (opt.bit_rate > 0)
            enc->bit_rate = opt.bit_rate;

        // the sample format: the input's, packed or planar, if the encoder takes it, else the encoder's first
        const enum AVSampleFormat *fmts = NULL;
#if LIBAVCODEC_VERSION_INT >= AV_VERSION_INT(61,13,100)
        avcodec_get_supported_config(NULL,codec,AV_CODEC_CONFIG_SAMPLE_FORMAT,0,(const void**)&fmts,NULL);
#else
        fmts = codec->sample_fmts;
#endif
        enc->sample_fmt = in.dec->sample_fmt;
        if (fmts != NULL) {
            const enum AVSampleFormat want[2] = { av_get_packed_sample_fmt(in.dec->sample_fmt),av_get_planar_sample_fmt(in.dec->sample_fmt) };
            enc->sample_fmt = fmts[0];
            for (int i=0;fmts[i] != AV_SAMPLE_FMT_NONE;i++) {
                if (fmts[i] == want[0] || fmts[i] == want[1]) {
                    enc->sample_fmt = fmts[i];
                    break;
                }
            }
        }
        if (in.dec->bits_per_raw_sample > 0 && av_get_bytes_per_sample(enc->sample_fmt) * 8 >= in.dec->bits_per_raw_sample)
            enc->bits_per_raw_sample = in.dec->bits_per_raw_sample;

        if (oc->oformat->flags & AVFMT_GLOBALHEADER)
            enc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if ((r = avcodec_open2(enc,codec,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",codec->name,averr(r).c_str());
            return false;
        }

        st = avformat_new_stream(oc,NULL);
        avcodec_parameters_from_context(st->codecpar,enc);
        st->time_base = enc->time_base;
        av_dict_copy(&oc->metadata,in.fmt->metadata,0);
        av_dict_copy(&st->metadata,in.fmt->streams[in.aindex]->metadata,0);
        av_dict_set(&oc->metadata,"encoder",NULL,0);

        frame_size = (enc->frame_size > 0 && !(codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) ? enc->frame_size : 4096;

        if (swr_alloc_set_opts2(&swr,&enc->ch_layout,enc->sample_fmt,in.rate,&enc->ch_layout,AV_SAMPLE_FMT_FLTP,in.rate,0,NULL) < 0)
            return false;
        if (opt.dither)
            av_opt_set_int(swr,"dither_method",SWR_DITHER_TRIANGULAR,0);
        if (swr_init(swr) < 0) {
            fprintf(stderr,"Cannot convert the audio to %s\n",av_get_sample_fmt_name(enc->sample_fmt));
            return false;
        }

        fifo = av_audio_fifo_alloc(enc->sample_fmt,enc->ch_layout.nb_channels,frame_size * 2);
        pkt = av_packet_alloc();

        if (!(oc->oformat->flags & AVFMT_NOFILE)) {
            if ((r = avio_open(&oc->pb,opt.output.c_str(),AVIO_FLAG_WRITE)) < 0) {
                fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
                return false;
            }
        }
        if ((r = avformat_write_header(oc,NULL)) < 0) {
            fprintf(stderr,"%s: %s\n",opt.output.c_str(),averr(r).c_str());
            return false;
        }
        header = true;

        return true;
    }

    bool write_encoded(void) {
        int r;

        while ((r = avcodec_receive_packet(enc,pkt)) >= 0) {
            av_packet_rescale_ts(pkt,enc->time_base,st->time_base);
            pkt->stream_index = st->index;
            if ((r = av_interleaved_write_frame(oc,pkt)) < 0) {
                fprintf(stderr,"Write error: %s\n",averr(r).c_str());
                return false;
            }
        }
        if (r != AVERROR(EAGAIN) && r != AVERROR_EOF) {
            fprintf(stderr,"Audio encoding error: %s\n",averr(r).c_str());
            return false;
        }

        return true;
    }

    // encodes the frames in the fifo, all of them at the end
    bool encode_fifo(bool last) {
        while (av_audio_fifo_size(fifo) >= frame_size || (last && av_audio_fifo_size(fifo) > 0)) {
            int n = std::min(av_audio_fifo_size(fifo),frame_size);
            AVFrame *f = av_frame_alloc();
            int r;

            f->nb_samples = (n < frame_size && !(enc->codec->capabilities & (AV_CODEC_CAP_SMALL_LAST_FRAME | AV_CODEC_CAP_VARIABLE_FRAME_SIZE))) ? frame_size : n;
            f->format = enc->sample_fmt;
            f->sample_rate = enc->sample_rate;
            av_channel_layout_copy(&f->ch_layout,&enc->ch_layout);
            if (av_frame_get_buffer(f,0) < 0) {
                av_frame_free(&f);
                return false;
            }
            if (f->nb_samples > n)
                av_samples_set_silence(f->extended_data,0,f->nb_samples,f->ch_layout.nb_channels,enc->sample_fmt);
            av_audio_fifo_read(fifo,(void**)f->extended_data,n);
            f->pts = pts;
            pts += f->nb_samples;

            r = avcodec_send_frame(enc,f);
            av_frame_free(&f);
            if (r < 0) {
                fprintf(stderr,"Audio encoding error: %s\n",averr(r).c_str());
                return false;
            }
            if (!write_encoded())
                return false;
        }

        return true;
    }

    // float planar samples out of the filter, n of each channel
    bool write(std::vector<float> *x,int n) {
        if (n <= 0)
            return true;

        const int ch = enc->ch_layout.nb_channels;
        std::vector<const uint8_t*> src(ch);
        uint8_t **dst = NULL;
        int r;

        for (int c=0;c < ch;c++)
            src[c] = (const uint8_t*)x[c].data();

        if (av_samples_alloc_array_and_samples(&dst,NULL,ch,n,enc->sample_fmt,0) < 0)
            return false;
        r = swr_convert(swr,dst,n,src.data(),n);
        if (r > 0)
            av_audio_fifo_write(fifo,(void**)dst,r);
        av_freep(&dst[0]);
        av_freep(&dst);
        if (r < 0)
            return false;

        return encode_fifo(false);
    }

    bool finish(void) {
        bool ok = encode_fifo(true);

        if (ok) {
            int r = avcodec_send_frame(enc,NULL);
            if (r < 0 && r != AVERROR_EOF) {
                fprintf(stderr,"Audio encoding error: %s\n",averr(r).c_str());
                ok = false;
            }
            else {
                ok = write_encoded();
            }
        }

        if (header) {
            int r = av_write_trailer(oc);
            if (r < 0) {
                fprintf(stderr,"Write error: %s\n",averr(r).c_str());
                ok = false;
            }
        }

        return ok;
    }
};

/////////////////////////////////////////////////////////////////////////////

static void progress(const options_t &opt,const char *what,const input_t &in,time_t &last,bool end) {
    if (opt.quiet)
        return;

    time_t now = time(NULL);
    if (!end && now == last)
        return;
    last = now;

    const double t = (double)in.done / in.rate;
    if (in.total > 0)
        fprintf(stderr,"\r%s: %02d:%02d:%02d  %5.1f%%",what,(int)(t / 3600),(int)(t / 60) % 60,(int)t % 60,(100.0 * in.done) / in.total);
    else
        fprintf(stderr,"\r%s: %02d:%02d:%02d",what,(int)(t / 3600),(int)(t / 60) % 60,(int)t % 60);
    if (end)
        fprintf(stderr,"\n");
    fflush(stderr);
}

static bool run(const options_t &opt) {
    derumble_t dr;
    time_t last = 0;

    // first pass: the level of the rumble
    {
        input_t in;
        int n;

        if (!in.open(opt))
            return false;
        if (!dr.init(opt.rumble,in.rate,in.channels)) {
            fprintf(stderr,"Cannot set up the filter\n");
            return false;
        }

        while (in.read(n)) {
            std::vector<const float*> x(in.channels);
            for (int c=0;c < in.channels;c++) x[c] = in.conv[c].data();
            dr.analyze(x.data(),n);
            progress(opt,"Finding the rumble",in,last,false);
        }
        progress(opt,"Finding the rumble",in,last,true);
        if (in.stopped) {
            fprintf(stderr,"Stopped by CTRL+C\n");
            return false;
        }
        dr.end_analysis();

        if (!opt.quiet) {
            fprintf(stderr,"%d samples to a transform, %.1fHz apart, %.1fms a hop\n",dr.fft,(double)in.rate / dr.fft,(1000.0 * dr.hop) / in.rate);
            for (int c=0;c < in.channels;c++)
                fprintf(stderr,"Channel %d: rumble %.1fdB (the middle of it over time, %g to %gHz)\n",c,dr.rumble_db(c),opt.rumble.highpass,opt.rumble.cutoff);
        }

        if (opt.report) {
            const double len = (double)in.done / in.rate;
            fprintf(stdout,"time     ");
            for (int c=0;c < in.channels;c++) fprintf(stdout,"  ch%d dB",c);
            fprintf(stdout,"\n");
            for (double t=0;t < len;t += 60) {
                fprintf(stdout,"%02d:%02d:%02d ",(int)(t / 3600),(int)(t / 60) % 60,(int)t % 60);
                for (int c=0;c < in.channels;c++) fprintf(stdout," %7.1f",dr.rumble_db_at(c,t + 30));
                fprintf(stdout,"\n");
            }
        }
    }

    // second pass: filter, encode
    input_t in;
    output_t out;
    int n;

    if (!in.open(opt) || !out.open(opt,in))
        return false;
    if (!opt.quiet)
        fprintf(stderr,"Writing %s, %s\n",opt.output.c_str(),out.enc->codec->name);

    catch_interrupt();

    std::vector<std::vector<float>> y(in.channels);
    bool ok = true;

    while (ok && in.read(n)) {
        std::vector<const float*> x(in.channels);
        for (int c=0;c < in.channels;c++) {
            x[c] = in.conv[c].data();
            y[c].clear();
        }
        dr.filter(x.data(),n,y.data());
        ok = out.write(y.data(),(int)y[0].size());
        progress(opt,"Filtering",in,last,false);
    }
    progress(opt,"Filtering",in,last,true);

    if (ok) {
        for (auto &v : y) v.clear();
        dr.end_filter(y.data());
        ok = out.write(y.data(),(int)y[0].size());
    }
    if (!out.finish())
        ok = false;

    if (!opt.quiet && ok) {
        for (int c=0;c < in.channels;c++)
            fprintf(stderr,"Channel %d: %g to %gHz turned down by %.1fdB (the middle of it over time)\n",c,opt.rumble.highpass,opt.rumble.cutoff,dr.reduced_db(c));
    }

    if (!ok)
        fprintf(stderr,"%s is not complete\n",opt.output.c_str());
    else if (in.stopped)
        fprintf(stderr,"Stopped by CTRL+C: %s has what was done before it, and is complete\n",opt.output.c_str());

    return ok;
}

/////////////////////////////////////////////////////////////////////////////

static bool parse_double(const char *s,double &v) {
    char *end = NULL;
    v = strtod(s,&end);
    return end != s && *end == 0;
}

// a bit rate, such as 192000, 192k or 1M
static bool parse_rate(const char *s,int64_t &v) {
    char *end = NULL;
    double d = strtod(s,&end);

    if (end == s)
        return false;
    if (*end == 'k' || *end == 'K') { d *= 1000; end++; }
    else if (*end == 'm' || *end == 'M') { d *= 1000000; end++; }
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
            const bool takes = (o == "cutoff" || o == "highpass" || o == "reduce" || o == "over" || o == "window" || o == "percentile" ||
                                o == "release" || o == "fft" || o == "c" || o == "b" || o == "ss" || o == "t");
            const char *v = NULL;
            double d;

            if (takes) {
                if (++i >= argc) {
                    fprintf(stderr,"%s needs a value\n",a);
                    return 1;
                }
                v = argv[i];
            }

            if (o == "cutoff") {
                if (!parse_double(v,d) || d <= 0 || d > 5000) {
                    fprintf(stderr,"-cutoff %s: must be more than 0, up to 5000\n",v);
                    return 1;
                }
                opt.rumble.cutoff = d;
            }
            else if (o == "highpass") {
                if (!parse_double(v,d) || d < 0 || d > 5000) {
                    fprintf(stderr,"-highpass %s: must be 0 to 5000\n",v);
                    return 1;
                }
                opt.rumble.highpass = d;
            }
            else if (o == "reduce") {
                if (!parse_double(v,d) || d < 0 || d > 200) {
                    fprintf(stderr,"-reduce %s: must be 0 to 200\n",v);
                    return 1;
                }
                opt.rumble.reduce = d;
            }
            else if (o == "over") {
                if (!parse_double(v,d) || d < 0 || d > 100) {
                    fprintf(stderr,"-over %s: must be 0 to 100\n",v);
                    return 1;
                }
                opt.rumble.over = d;
            }
            else if (o == "window") {
                if (!parse_double(v,d) || d < 0.5 || d > 3600) {
                    fprintf(stderr,"-window %s: must be 0.5 to 3600\n",v);
                    return 1;
                }
                opt.rumble.window = d;
            }
            else if (o == "percentile") {
                if (!parse_double(v,d) || d < 0 || d > 100) {
                    fprintf(stderr,"-percentile %s: must be 0 to 100\n",v);
                    return 1;
                }
                opt.rumble.percentile = d;
            }
            else if (o == "release") {
                if (!parse_double(v,d) || d < 0 || d > 10) {
                    fprintf(stderr,"-release %s: must be 0 to 10\n",v);
                    return 1;
                }
                opt.rumble.release = d;
            }
            else if (o == "fft") {
                if (!parse_double(v,d) || d < 256 || d > 65536 || d != floor(d) || ((int)d & ((int)d - 1)) != 0) {
                    fprintf(stderr,"-fft %s: must be a power of 2, 256 to 65536\n",v);
                    return 1;
                }
                opt.rumble.fft = (int)d;
            }
            else if (o == "c") {
                opt.codec = v;
            }
            else if (o == "b") {
                if (!parse_rate(v,opt.bit_rate)) {
                    fprintf(stderr,"-b %s: not a bit rate\n",v);
                    return 1;
                }
            }
            else if (o == "ss") {
                if (!parse_time(v,opt.start_us)) {
                    fprintf(stderr,"-ss %s: not a time\n",v);
                    return 1;
                }
            }
            else if (o == "t") {
                if (!parse_time(v,opt.length_us)) {
                    fprintf(stderr,"-t %s: not a time\n",v);
                    return 1;
                }
            }
            else if (o == "report") {
                opt.report = true;
            }
            else if (o == "dither") {
                opt.dither = true;
            }
            else if (o == "y") {
                opt.overwrite = true;
            }
            else if (o == "q") {
                opt.quiet = true;
            }
            else if (o == "h" || o == "help" || o == "-help") {
                help();
                return 0;
            }
            else {
                fprintf(stderr,"Unknown option %s\n",a);
                return 1;
            }
        }
        else {
            files.push_back(a);
        }
    }

    if (files.size() != 2) {
        help();
        return 1;
    }
    opt.input = files[0];
    opt.output = files[1];

    if (opt.rumble.highpass >= opt.rumble.cutoff) {
        fprintf(stderr,"-highpass must be less than -cutoff\n");
        return 1;
    }

    if (!opt.overwrite) {
        struct stat s;
        if (stat(opt.output.c_str(),&s) == 0) {
            fprintf(stderr,"%s is already there, -y to overwrite it\n",opt.output.c_str());
            return 1;
        }
    }

    av_log_set_level(AV_LOG_ERROR);

    return run(opt) ? 0 : 1;
}
