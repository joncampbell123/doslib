#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include <map>
#include <random>
#include <string>
#include <vector>

#include <zlib.h>

#include "project.h"
#include "prproj_template.h"

/////////////////////////////////////////////////////////////////////////////
// names and paths

static std::string xml_escape(const std::string &s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '&': r += "&amp;"; break;
            case '<': r += "&lt;"; break;
            case '>': r += "&gt;"; break;
            case '"': r += "&quot;"; break;
            case '\'': r += "&apos;"; break;
            default: r += c; break;
        }
    }
    return r;
}

static std::string absolute(const std::string &path) {
    char *real = realpath(path.c_str(),NULL);
    if (real == NULL)
        return path;
    std::string r = real;
    free(real);
    return r;
}

static std::string base_name(const std::string &path) {
    const size_t s = path.rfind('/');
    return s == std::string::npos ? path : path.substr(s + 1u);
}

static std::string dir_name(const std::string &path) {
    const size_t s = path.rfind('/');
    return s == std::string::npos ? std::string(".") : (s == 0 ? std::string("/") : path.substr(0,s));
}

static bool windows_path(const std::string &p) {
    return (p.size() >= 2u && isalpha((unsigned char)p[0]) && p[1] == ':') || p.compare(0,2,"\\\\") == 0;
}

// a path as the editor finds it: with the first prefix of the map that is the start of it changed, and
// with backslashes if that makes it a Windows path
static std::string map_path(const std::string &path,const project_t &p) {
    std::string r = path;
    for (const auto &m : p.path_map) {
        const std::string &from = m.first;
        if (!from.empty() && r.compare(0,from.size(),from) == 0 &&
            (r.size() == from.size() || from.back() == '/' || r[from.size()] == '/')) {
            r = m.second + r.substr(from.size());
            break;
        }
    }
    if (windows_path(r)) {
        for (char &c : r) {
            if (c == '/')
                c = '\\';
        }
    }
    return r;
}

// the path of a file from the directory the project is in, as Premiere Pro keeps it: .\name, ..\dir\name
static std::string relative_path(const std::string &file,const std::string &project_dir,bool windows) {
    std::vector<std::string> a,b;
    auto split = [](const std::string &s,std::vector<std::string> &v) {
        size_t i = 0;
        while (i < s.size()) {
            size_t j = s.find('/',i);
            if (j == std::string::npos) j = s.size();
            if (j > i) v.push_back(s.substr(i,j - i));
            i = j + 1u;
        }
    };
    split(file,a);
    split(project_dir,b);
    size_t k = 0;
    while (k < a.size() && k < b.size() && a[k] == b[k])
        k++;
    const char *sep = windows ? "\\" : "/";
    std::string r = k == b.size() ? std::string(".") + sep : std::string();
    for (size_t i=k;i < b.size();i++)
        r += std::string("..") + sep;
    for (size_t i=k;i < a.size();i++)
        r += a[i] + (i + 1u < a.size() ? sep : "");
    return r;
}

/////////////////////////////////////////////////////////////////////////////
// time

// Premiere Pro counts time in ticks, 254016000000 a second, which every common frame rate and sample rate
// divides into
static const int64_t TICKS = 254016000000ll;

static int64_t ticks_per(int num,int den) {
    return (int64_t)llround((long double)TICKS * (long double)den / (long double)num);
}

/////////////////////////////////////////////////////////////////////////////
// Premiere Pro

static const char *MEDIA_VIDEO = "228cda18-3625-4d2d-951e-348879e4ed93";
static const char *MEDIA_AUDIO = "80b8e3d5-6dca-4195-aefb-cb5f407ab009";
static const char *MEDIA_DATA = "d8143ffe-eec4-4d2a-a909-d5f7bf094dc5";
static const char *STEREO_LAYOUT = "[{&quot;channellabel&quot;:100},{&quot;channellabel&quot;:101}]";
static const char *START_KEYFRAME = "-91445760000000000";   // a parameter that does not change: its keyframe before any time

class prproj_writer_t {
public:
    prproj_writer_t(const project_t &p,const std::string &path) : p(p), caps(*p.caps), path(path) {
        std::random_device rd;
        rng.seed(((uint64_t)rd() << 32) ^ rd());
    }

    std::string write(void) {
        const int64_t fticks = ticks_per(p.rate_num,p.rate_den);
        const std::string seq_uid = uuid(),root_uid = uuid(),project_guid = uuid();

        out = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<PremiereData Version=\"3\">\n\t<Project ObjectRef=\"1\"/>\n";
        std::string proj = PRPROJ_PROJECT;
        replace(proj,"@SEQUENCE@",seq_uid);
        replace(proj,"@PROJECT@",project_guid);
        replace(proj,"@ROOT@",root_uid);
        out += proj;
        out += PRPROJ_SETTINGS;

        // each capture, and the clip of it in the project
        std::vector<media_t> media(caps.size());
        std::vector<bool> used(caps.size(),false);
        for (const track_t &t : p.video)
            for (const clip_t &c : t.clips) used[c.cap] = true;
        for (const track_t &t : p.audio)
            for (const clip_t &c : t.clips) used[c.cap] = true;
        std::vector<std::string> items;
        for (size_t i=0;i < caps.size();i++) {
            if (used[i]) {
                write_media(media[i],caps[i]);
                items.push_back(media[i].item_uid);
            }
        }

        // the sequence, and the clip of it in the project
        const std::string seq_item = write_sequence_clip(seq_uid,p.frames * fticks);
        items.insert(items.begin(),seq_item);
        write_sequence(seq_uid,media);

        out += "\t<RootProjectItem ObjectUID=\"" + root_uid + "\" ClassID=\"1c307a89-9318-47d7-a583-bf2553736543\" Version=\"1\">\n"
               "\t\t<ProjectItem Version=\"1\">\n"
               "\t\t\t<Node Version=\"1\">\n"
               "\t\t\t\t<Properties Version=\"1\">\n"
               "\t\t\t\t\t<ProjectViewState.ID>a594d17e-cd6c-4480-a86c-a9aa6d98a473</ProjectViewState.ID>\n"
               "\t\t\t\t</Properties>\n"
               "\t\t\t\t<ID>1000000</ID>\n"
               "\t\t\t</Node>\n"
               "\t\t\t<Name>Root Bin</Name>\n"
               "\t\t</ProjectItem>\n"
               "\t\t<ProjectItemContainer Version=\"1\">\n"
               "\t\t\t<Items Version=\"1\">\n";
        for (size_t i=0;i < items.size();i++)
            out += "\t\t\t\t<Item Index=\"" + num(i) + "\" ObjectURef=\"" + items[i] + "\"/>\n";
        out += "\t\t\t</Items>\n"
               "\t\t</ProjectItemContainer>\n"
               "\t</RootProjectItem>\n"
               "</PremiereData>\n";
        return out;
    }
private:
    // what is written of a capture, for the clips of it to refer to
    struct media_t {
        std::string             media_uid,master_uid,item_uid;
        unsigned int            video_source = 0,audio_source = 0,markers = 0;
        bool                    audio = false;
    };

    static void replace(std::string &s,const std::string &what,const std::string &with) {
        size_t i = 0;
        while ((i=s.find(what,i)) != std::string::npos) {
            s.replace(i,what.size(),with);
            i += with.size();
        }
    }

    template <class T> static std::string num(T v) {
        return std::to_string(v);
    }

    std::string uuid(void) {
        const uint64_t a = rng(),b = rng();
        char buf[40];
        snprintf(buf,sizeof(buf),"%08x-%04x-4%03x-%04x-%012llx",(unsigned int)(a >> 32),(unsigned int)((a >> 16) & 0xFFFF),
            (unsigned int)(a & 0xFFF),(unsigned int)(0x8000 | ((b >> 48) & 0x3FFF)),(unsigned long long)(b & 0xFFFFFFFFFFFFull));
        return buf;
    }

    unsigned int id(void) {
        return next_id++;
    }

    std::string obj(const char *tag,unsigned int oid,const char *cls,int version) {
        return std::string("\t<") + tag + " ObjectID=\"" + num(oid) + "\" ClassID=\"" + cls + "\" Version=\"" + num(version) + "\">\n";
    }

    std::string uobj(const char *tag,const std::string &uid,const char *cls,int version) {
        return std::string("\t<") + tag + " ObjectUID=\"" + uid + "\" ClassID=\"" + cls + "\" Version=\"" + num(version) + "\">\n";
    }

    static std::string ref(int tabs,const char *tag,unsigned int oid) {
        return std::string((size_t)tabs,'\t') + "<" + tag + " ObjectRef=\"" + num(oid) + "\"/>\n";
    }

    static std::string uref(int tabs,const char *tag,const std::string &uid) {
        return std::string((size_t)tabs,'\t') + "<" + tag + " ObjectURef=\"" + uid + "\"/>\n";
    }

    static std::string val(int tabs,const char *tag,const std::string &v) {
        return std::string((size_t)tabs,'\t') + "<" + tag + ">" + v + "</" + tag + ">\n";
    }

    // the duration of a capture in ticks: to the end of its last frame
    int64_t duration(const capture_t &c) const {
        return (c.position(c.frames() - 1u) + 1) * ticks_per(c.rate_num,c.rate_den);
    }

    void write_clip_channels(unsigned int group) {
        const unsigned int vec = id(),c0 = id(),c1 = id();
        out += obj("ClipChannelGroupVectorSerializer",group,"a3127a8c-95d4-456e-a7f5-171b3f922426",1);
        out += "\t\t<ClipChannelVectors Version=\"1\">\n"
               "\t\t\t<ClipChannelVectorItem Index=\"0\" ObjectRef=\"" + num(vec) + "\"/>\n"
               "\t\t</ClipChannelVectors>\n"
               "\t</ClipChannelGroupVectorSerializer>\n";
        out += obj("ClipChannelVectorSerializer",vec,"333d203b-3a53-4195-8894-fc7523ff3dc7",1);
        out += "\t\t<ClipChannels Version=\"1\">\n"
               "\t\t\t<ClipChannelItem Index=\"0\" ObjectRef=\"" + num(c0) + "\"/>\n"
               "\t\t\t<ClipChannelItem Index=\"1\" ObjectRef=\"" + num(c1) + "\"/>\n"
               "\t\t</ClipChannels>\n"
               "\t\t<ChannelType>1</ChannelType>\n"
               "\t</ClipChannelVectorSerializer>\n";
        for (unsigned int k=0;k < 2u;k++) {
            out += obj("ClipChannelSerializer",k ? c1 : c0,"5c89aa7a-89a6-4483-becd-f2b1def42316",1);
            out += val(2,"SourceClipIndex","0") + val(2,"mSourceChannelIndex",num(k)) + "\t</ClipChannelSerializer>\n";
        }
    }

    // the two channels of a source of audio, for an audio clip of it
    void write_secondary(unsigned int sc[2],unsigned int source) {
        for (unsigned int k=0;k < 2u;k++) {
            out += obj("SecondaryContent",sc[k],"f9d004b5-cb04-4e2f-af6f-64fadc2c4be9",1);
            out += ref(2,"Content",source) + val(2,"ChannelIndex",num(k)) + "\t</SecondaryContent>\n";
        }
    }

    void write_audio_chain(unsigned int oid,bool clip) {
        out += obj("AudioComponentChain",oid,"3cb131d1-d3c0-47ae-a19a-bdf75ea11674",4);
        out += "\t\t<DefaultVol>true</DefaultVol>\n"
               "\t\t<DefaultVolumeComponentID>1</DefaultVolumeComponentID>\n"
               "\t\t<DefaultChannelVolumeComponentID>2</DefaultChannelVolumeComponentID>\n"
               "\t\t<ComponentChain Version=\"3\">\n";
        if (clip)
            out += "\t\t\t<Node Version=\"1\">\n"
                   "\t\t\t\t<Properties Version=\"1\">\n"
                   "\t\t\t\t\t<MZ.ComponentChain.ActiveComponentID>1</MZ.ComponentChain.ActiveComponentID>\n"
                   "\t\t\t\t\t<MZ.ComponentChain.ActiveComponentParamIndex>4294967295</MZ.ComponentChain.ActiveComponentParamIndex>\n"
                   "\t\t\t\t</Properties>\n"
                   "\t\t\t</Node>\n";
        out += "\t\t</ComponentChain>\n";
        out += val(2,"AudioChannelLayout",STEREO_LAYOUT);
        out += "\t\t<ChannelType>1</ChannelType>\n\t</AudioComponentChain>\n";
    }

    // the clip of the audio or video of a source, from in to out (ticks)
    void write_clip(bool audio,unsigned int oid,unsigned int source,unsigned int markers,int64_t in,int64_t outp,bool master,int label) {
        unsigned int sc[2] = { 0,0 };
        if (audio) {
            sc[0] = id();
            sc[1] = id();
        }
        out += audio ? obj("AudioClip",oid,"b8830d03-de02-41ee-84ec-fe566dc70cd9",8) : obj("VideoClip",oid,"9308dbef-2440-4acb-9ab2-953b9a4e82ec",11);
        out += "\t\t<Clip Version=\"18\">\n"
               "\t\t\t<Node Version=\"1\">\n"
               "\t\t\t\t<Properties Version=\"1\">\n";
        out += label == 5 ? "\t\t\t\t\t<asl.clip.label.color>19005</asl.clip.label.color>\n"
                            "\t\t\t\t\t<asl.clip.label.name>BE.Prefs.LabelColors.5</asl.clip.label.name>\n"
                          : "\t\t\t\t\t<asl.clip.label.color>6769408</asl.clip.label.color>\n"
                            "\t\t\t\t\t<asl.clip.label.name>BE.Prefs.LabelColors.1</asl.clip.label.name>\n";
        out += "\t\t\t\t</Properties>\n"
               "\t\t\t</Node>\n";
        if (markers != 0)
            out += "\t\t\t<MarkerOwner Version=\"1\">\n" + ref(4,"Markers",markers) + "\t\t\t</MarkerOwner>\n";
        out += ref(3,"Source",source);
        if (in >= 0) {
            out += val(3,"OutPoint",num(outp));
            out += val(3,"InPoint",num(in));
        }
        out += val(3,"ClipID",uuid());
        if (master)
            out += "\t\t\t<InUse>false</InUse>\n";
        out += "\t\t</Clip>\n";
        if (audio) {
            out += "\t\t<SecondaryContents Version=\"1\">\n"
                   "\t\t\t<SecondaryContentItem Index=\"0\" ObjectRef=\"" + num(sc[0]) + "\"/>\n"
                   "\t\t\t<SecondaryContentItem Index=\"1\" ObjectRef=\"" + num(sc[1]) + "\"/>\n"
                   "\t\t</SecondaryContents>\n";
            out += val(2,"AudioChannelLayout",STEREO_LAYOUT);
            out += "\t</AudioClip>\n";
            write_secondary(sc,source);
        }
        else {
            out += "\t</VideoClip>\n";
        }
    }

    void write_media(media_t &m,const capture_t &c) {
        const std::string abs = absolute(c.path);
        const std::string fpath = map_path(abs,p);
        const std::string rel = relative_path(abs,dir_name(absolute(path)),windows_path(fpath));
        const std::string name = xml_escape(base_name(c.path));
        const int64_t dur = duration(c);
        const int64_t fticks = ticks_per(c.rate_num,c.rate_den);
        const unsigned int vstream = id(),astream = id(),logging = id(),chain = id(),vclip = id(),aclip = id(),groups = id();

        m.audio = c.audio_rate > 0 && c.audio_channels > 0;
        m.media_uid = uuid();
        m.master_uid = uuid();
        m.item_uid = uuid();
        m.video_source = id();
        m.audio_source = id();
        m.markers = id();

        out += uobj("ClipProjectItem",m.item_uid,"cb4e0ed7-aca1-4171-8525-e3658dec06dd",1);
        out += "\t\t<ProjectItem Version=\"1\">\n"
               "\t\t\t<Node Version=\"1\">\n"
               "\t\t\t\t<Properties Version=\"1\">\n"
               "\t\t\t\t\t<Column.PropertyText.Label>BE.Prefs.LabelColors.1</Column.PropertyText.Label>\n"
               "\t\t\t\t</Properties>\n"
               "\t\t\t</Node>\n";
        out += val(3,"Name",name);
        out += "\t\t</ProjectItem>\n";
        out += uref(2,"MasterClip",m.master_uid);
        out += "\t</ClipProjectItem>\n";

        out += uobj("MasterClip",m.master_uid,"fb11c33a-b0a9-4465-aa94-b6d5db2628cf",12);
        out += ref(2,"LoggingInfo",logging);
        if (m.audio)
            out += "\t\t<AudioComponentChains Version=\"1\">\n" + ref(3,"AudioComponentChain",chain) + "\t\t</AudioComponentChains>\n";
        out += "\t\t<Clips Version=\"1\">\n";
        out += "\t\t\t<Clip Index=\"0\" ObjectRef=\"" + num(vclip) + "\"/>\n";
        if (m.audio)
            out += "\t\t\t<Clip Index=\"1\" ObjectRef=\"" + num(aclip) + "\"/>\n";
        out += "\t\t</Clips>\n";
        out += ref(2,"AudioClipChannelGroups",groups);
        out += val(2,"Name",name);
        out += "\t\t<MasterClipChangeVersion>1</MasterClipChangeVersion>\n";
        out += "\t</MasterClip>\n";

        out += obj("ClipLoggingInfo",logging,"77ab7fdd-dcdf-465d-9906-7a330ca1e738",10);
        out += val(2,"ClipName",name);
        if (c.rate_num == 60000 && c.rate_den == 1001)
            out += "\t\t<TimecodeFormat>107</TimecodeFormat>\n";
        out += "\t\t<MediaInPoint>0</MediaInPoint>\n";
        out += val(2,"MediaOutPoint",num(dur));
        out += val(2,"MediaFrameRate",num(fticks));
        out += "\t</ClipLoggingInfo>\n";

        if (m.audio)
            write_audio_chain(chain,false);
        write_clip(false,vclip,m.video_source,m.markers,0,dur,true,1);
        if (m.audio)
            write_clip(true,aclip,m.audio_source,m.markers,0,dur,true,1);
        if (m.audio) {
            write_clip_channels(groups);
        }
        else {
            out += obj("ClipChannelGroupVectorSerializer",groups,"a3127a8c-95d4-456e-a7f5-171b3f922426",1);
            out += "\t</ClipChannelGroupVectorSerializer>\n";
        }

        out += obj("Markers",m.markers,"bee50706-b524-416c-9f03-b596ce5f6866",4);
        out += "\t\t<ByGUID>byGUID</ByGUID>\n"
               "\t\t<LastMetadataState>00000000-0000-0000-0000-000000000000</LastMetadataState>\n"
               "\t</Markers>\n";

        for (unsigned int k=0;k < (m.audio ? 2u : 1u);k++) {
            out += k ? obj("AudioMediaSource",m.audio_source,"f588da05-fc2a-4fbc-9383-74d653b379e3",2)
                     : obj("VideoMediaSource",m.video_source,"e64ddf74-8fac-4682-8aa8-0e0ca2248949",2);
            out += "\t\t<MediaSource Version=\"4\">\n"
                   "\t\t\t<Content Version=\"10\">\n"
                   "\t\t\t</Content>\n";
            out += uref(3,"Media",m.media_uid);
            out += "\t\t</MediaSource>\n";
            out += val(2,"OriginalDuration",num(dur));
            out += k ? "\t</AudioMediaSource>\n" : "\t</VideoMediaSource>\n";
        }

        out += uobj("Media",m.media_uid,"7a5c103e-f3ac-4391-b6b4-7cc3d2f9a7ff",30);
        if (m.audio)
            out += ref(2,"AudioStream",astream);
        out += ref(2,"VideoStream",vstream);
        out += val(2,"RelativePath",xml_escape(rel));
        out += val(2,"FilePath",xml_escape(fpath));
        out += "\t\t<ImplementationID>1fa18bfa-255c-44b1-ad73-56bcd99fceaf</ImplementationID>\n";
        out += val(2,"Title",name);
        out += val(2,"FileKey",uuid());
        if (m.audio)
            out += val(2,"ConformedAudioRate",num(ticks_per(c.audio_rate,1)));
        out += val(2,"ActualMediaFilePath",xml_escape(fpath));
        out += "\t</Media>\n";

        if (m.audio) {
            out += obj("AudioStream",astream,"0b5cf52f-2b85-4863-890b-8844b64ecfe9",8);
            out += val(2,"FrameRate",num(ticks_per(c.audio_rate,1)));
            out += val(2,"AudioChannelLayout",STEREO_LAYOUT);
            out += val(2,"Duration",num(dur));
            out += "\t\t<SampleType>3</SampleType>\n";
            out += "\t</AudioStream>\n";
        }
        out += obj("VideoStream",vstream,"a36e4719-3ec6-4a0c-ab11-8b4aab377aa5",22);
        out += val(2,"FrameRate",num(fticks));
        out += val(2,"Duration",num(dur));
        out += "\t\t<IgnoreAlpha>true</IgnoreAlpha>\n";
        out += val(2,"FrameRect","0,0," + num(c.width) + "," + num(c.height));
        if (c.sar_num != c.sar_den)
            out += val(2,"OriginalPAR",num(c.sar_num) + "," + num(c.sar_den));
        out += "\t\t<FieldTypeIsUncertain>true</FieldTypeIsUncertain>\n";
        out += "\t</VideoStream>\n";
    }

    // the sequence as a clip in the project
    std::string write_sequence_clip(const std::string &seq_uid,int64_t dur) {
        const std::string item = uuid(),master = uuid();
        const unsigned int logging = id(),chain = id(),aclip = id(),vclip = id(),groups = id();
        const unsigned int asrc = id(),vsrc = id();
        const std::string name = xml_escape(p.name);

        out += uobj("ClipProjectItem",item,"cb4e0ed7-aca1-4171-8525-e3658dec06dd",1);
        out += "\t\t<ProjectItem Version=\"1\">\n"
               "\t\t\t<Node Version=\"1\">\n"
               "\t\t\t\t<Properties Version=\"1\">\n"
               "\t\t\t\t\t<Column.PropertyText.Label>BE.Prefs.LabelColors.5</Column.PropertyText.Label>\n"
               "\t\t\t\t</Properties>\n"
               "\t\t\t</Node>\n";
        out += val(3,"Name",name);
        out += "\t\t</ProjectItem>\n";
        out += uref(2,"MasterClip",master);
        out += "\t</ClipProjectItem>\n";

        out += uobj("MasterClip",master,"fb11c33a-b0a9-4465-aa94-b6d5db2628cf",12);
        out += ref(2,"LoggingInfo",logging);
        out += "\t\t<AudioComponentChains Version=\"1\">\n" + ref(3,"AudioComponentChain",chain) + "\t\t</AudioComponentChains>\n";
        out += "\t\t<Clips Version=\"1\">\n"
               "\t\t\t<Clip Index=\"0\" ObjectRef=\"" + num(aclip) + "\"/>\n"
               "\t\t\t<Clip Index=\"1\" ObjectRef=\"" + num(vclip) + "\"/>\n"
               "\t\t</Clips>\n";
        out += ref(2,"AudioClipChannelGroups",groups);
        out += val(2,"Name",name);
        out += "\t\t<MasterClipChangeVersion>1</MasterClipChangeVersion>\n";
        out += "\t</MasterClip>\n";

        out += obj("ClipLoggingInfo",logging,"77ab7fdd-dcdf-465d-9906-7a330ca1e738",10);
        out += val(2,"ClipName",name);
        out += "\t</ClipLoggingInfo>\n";
        write_audio_chain(chain,false);
        write_clip(true,aclip,asrc,0,-1,-1,true,5);
        write_clip(false,vclip,vsrc,0,-1,-1,true,5);
        write_clip_channels(groups);

        out += obj("AudioSequenceSource",asrc,"e8d4cc83-38cb-491f-9d94-e5f7e3b205ee",7);
        out += "\t\t<SequenceSource Version=\"4\">\n"
               "\t\t\t<Content Version=\"10\">\n"
               "\t\t\t</Content>\n" + uref(3,"Sequence",seq_uid) + "\t\t</SequenceSource>\n";
        out += val(2,"OriginalDuration",num(dur));
        out += "\t</AudioSequenceSource>\n";
        out += obj("VideoSequenceSource",vsrc,"4752dfa9-7a7e-4a3b-a25b-cafde1a8d036",3);
        out += "\t\t<SequenceSource Version=\"4\">\n"
               "\t\t\t<Content Version=\"10\">\n"
               "\t\t\t</Content>\n" + uref(3,"Sequence",seq_uid) + "\t\t</SequenceSource>\n";
        out += val(2,"OriginalDuration",num(dur));
        out += "\t</VideoSequenceSource>\n";
        return item;
    }

    // the properties of a track, as the timeline shows it
    std::string track_head(unsigned int track_id,const char *media,unsigned int index,bool audio) {
        std::string s = "\t\t\t<Track Version=\"4\">\n"
                        "\t\t\t\t<Node Version=\"1\">\n"
                        "\t\t\t\t\t<Properties Version=\"1\">\n"
                        "\t\t\t\t\t\t<TL.SQTrackExpanded>0</TL.SQTrackExpanded>\n"
                        "\t\t\t\t\t\t<TL.SQTrackExpandedHeight>17</TL.SQTrackExpandedHeight>\n"
                        "\t\t\t\t\t\t<MZ.SourceTrackState>0</MZ.SourceTrackState>\n";
        s += "\t\t\t\t\t\t<MZ.SourceTrackNumber>" + std::string(index == 0 ? "0" : "-1") + "</MZ.SourceTrackNumber>\n";
        s += "\t\t\t\t\t\t<MZ.TrackTargeted>" + std::string(index == 0 ? "1" : "0") + "</MZ.TrackTargeted>\n";
        s += "\t\t\t\t\t\t<TL.SQTrackShy>0</TL.SQTrackShy>\n";
        if (audio)
            s += "\t\t\t\t\t\t<TL.SQTrackAudioKeyframeStyle>0</TL.SQTrackAudioKeyframeStyle>\n"
                 "\t\t\t\t\t\t<CM.KeyframeMode>true</CM.KeyframeMode>\n";
        s += "\t\t\t\t\t</Properties>\n"
             "\t\t\t\t</Node>\n";
        s += val(4,"ID",num(track_id)) + val(4,"MediaType",media) + val(4,"Index",num(index));
        s += "\t\t\t</Track>\n";
        return s;
    }

    std::string track_items(const std::vector<unsigned int> &items,const char *media,unsigned int index) {
        std::string s = "\t\t\t<ClipItems Version=\"3\">\n";
        if (!items.empty()) {
            s += "\t\t\t\t<TrackItems Version=\"1\">\n";
            for (size_t i=0;i < items.size();i++)
                s += "\t\t\t\t\t<TrackItem Index=\"" + num(i) + "\" ObjectRef=\"" + num(items[i]) + "\"/>\n";
            s += "\t\t\t\t</TrackItems>\n";
        }
        s += val(4,"MediaType",media) + val(4,"Index",num(index));
        s += "\t\t\t</ClipItems>\n"
             "\t\t\t<TransitionItems Version=\"3\">\n";
        s += val(4,"MediaType",media) + val(4,"Index",num(index));
        s += "\t\t\t</TransitionItems>\n";
        return s;
    }

    // the Motion effect each clip on a video track has, as it is when nothing is changed
    void write_motion(unsigned int oid) {
        unsigned int prm[11];
        for (unsigned int &x : prm)
            x = id();
        out += obj("VideoFilterComponent",oid,"d10da199-beea-4dd1-b941-ed3a78766d50",9);
        out += "\t\t<Component Version=\"7\">\n\t\t\t<Params Version=\"1\">\n";
        for (int i=0;i < 11;i++)
            out += "\t\t\t\t<Param Index=\"" + num(i) + "\" ObjectRef=\"" + num(prm[i]) + "\"/>\n";
        out += "\t\t\t</Params>\n"
               "\t\t\t<ID>1</ID>\n"
               "\t\t\t<DisplayName>Motion</DisplayName>\n"
               "\t\t\t<Intrinsic>true</Intrinsic>\n"
               "\t\t</Component>\n"
               "\t\t<PremiereFilterPrivateData Encoding=\"base64\" BinaryHash=\"5b034ebf-91cc-882a-ba67-ade80000000e\">AAA=\n"
               "\t\t</PremiereFilterPrivateData>\n"
               "\t\t<MatchName>AE.ADBE Motion</MatchName>\n"
               "\t\t<VideoFilterType>2</VideoFilterType>\n"
               "\t</VideoFilterComponent>\n";

        auto point = [&](unsigned int pid,const char *name,int n) {
            out += obj("PointComponentParam",pid,"ca81d347-309b-44d2-acc7-1c572efb973c",4);
            out += val(2,"Name",name) + val(2,"ParameterID",num(n));
            out += val(2,"StartKeyframe",std::string(START_KEYFRAME) + ",0.5:0.5,0,0,0,0,0,0,5,4,0,0,0,0");
            out += "\t</PointComponentParam>\n";
        };
        auto number = [&](unsigned int pid,const char *cls,const char *name,int n,const char *v,const char *lo,const char *hi,const char *extra) {
            out += obj("VideoComponentParam",pid,cls,10);
            out += val(2,"Name",name);
            out += extra;
            out += val(2,"ParameterID",num(n));
            out += val(2,"StartKeyframe",std::string(START_KEYFRAME) + "," + v + ",0,0,0,0,0,0");
            if (lo != NULL)
                out += val(2,"LowerBound",lo) + val(2,"UpperBound",hi);
            out += "\t</VideoComponentParam>\n";
        };
        const char *F = "fe47129e-6c94-4fc0-95d5-c056a517aaf3";
        point(prm[0],"Position",1);
        number(prm[1],F,"Scale",2,"100.","0","10000","\t\t<UpperUIBound>200</UpperUIBound>\n");
        number(prm[2],F,"Scale Width",3,"100.","0","10000","\t\t<UpperUIBound>200</UpperUIBound>\n");
        number(prm[3],"cc12343e-f113-4d3b-ae05-b287db77d461"," ",4,"true",NULL,NULL,"");
        number(prm[4],F,"Rotation",5,"0.","-32768","32767","\t\t<ParameterControlType>3</ParameterControlType>\n");
        point(prm[5],"Anchor Point",6);
        number(prm[6],"a4ff2d6e-7ac2-44f8-9d52-17d9ca50e542","Anti-flicker Filter",7,"0.","0","1","");
        number(prm[7],F,"Crop Left",8,"0.","0","100","");
        number(prm[8],F,"Crop Top",9,"0.","0","100","");
        number(prm[9],F,"Crop Right",10,"0.","0","100","");
        number(prm[10],F,"Crop Bottom",11,"0.","0","100","");
    }

    // a clip on a track of the sequence: its own clip of the source, from where it starts in the capture
    unsigned int write_track_item(bool audio,const clip_t &c,const std::vector<media_t> &media,int64_t fticks) {
        const capture_t &cap = caps[c.cap];
        const media_t &m = media[c.cap];
        const unsigned int item = id(),chain = id(),sub = id(),clip = id();
        const int64_t cticks = ticks_per(cap.rate_num,cap.rate_den);
        const int64_t in = cap.position((size_t)c.frame) * cticks;

        if (audio) {
            out += obj("AudioClipTrackItem",item,"064ec682-9ba6-11d5-af2d-9ca32c7d6164",11);
        }
        else {
            out += obj("VideoClipTrackItem",item,"368b0406-29e3-4923-9fcd-094fbf9a1089",8);
        }
        out += "\t\t<ClipTrackItem Version=\"8\">\n"
               "\t\t\t<ComponentOwner Version=\"1\">\n" + ref(4,"Components",chain) + "\t\t\t</ComponentOwner>\n"
               "\t\t\t<TrackItem Version=\"4\">\n";
        if (c.start != 0)
            out += val(4,"Start",num(c.start * fticks));
        out += val(4,"End",num((c.start + c.length) * fticks));
        out += "\t\t\t</TrackItem>\n";
        out += ref(3,"SubClip",sub);
        out += "\t\t</ClipTrackItem>\n";
        if (audio) {
            out += val(2,"ID",uuid());
            out += "\t\t<PreRenderComponentChainHashVersion>1</PreRenderComponentChainHashVersion>\n";
            out += "\t</AudioClipTrackItem>\n";
            write_audio_chain(chain,true);
        }
        else {
            out += "\t\t<PixelAspectRatio>1,1</PixelAspectRatio>\n";
            out += "\t\t<ToneMapSettings>{&quot;peak&quot;:-1,&quot;version&quot;:3}</ToneMapSettings>\n";
            out += val(2,"FrameRect","0,0," + num(p.width) + "," + num(p.height));
            out += "\t</VideoClipTrackItem>\n";
            const unsigned int motion = id();
            out += obj("VideoComponentChain",chain,"0970e08a-f58f-4108-b29a-1a717b8e12e2",3);
            out += "\t\t<DefaultOpacity>true</DefaultOpacity>\n"
                   "\t\t<DefaultOpacityComponentID>2</DefaultOpacityComponentID>\n"
                   "\t\t<ComponentChain Version=\"3\">\n"
                   "\t\t\t<Node Version=\"1\">\n"
                   "\t\t\t\t<Properties Version=\"1\">\n"
                   "\t\t\t\t\t<MZ.ComponentChain.ActiveComponentID>1</MZ.ComponentChain.ActiveComponentID>\n"
                   "\t\t\t\t\t<MZ.ComponentChain.ActiveComponentParamIndex>4294967295</MZ.ComponentChain.ActiveComponentParamIndex>\n"
                   "\t\t\t\t</Properties>\n"
                   "\t\t\t</Node>\n"
                   "\t\t\t<Components Version=\"1\">\n";
            out += "\t\t\t\t<Component Index=\"0\" ObjectRef=\"" + num(motion) + "\"/>\n";
            out += "\t\t\t</Components>\n"
                   "\t\t</ComponentChain>\n"
                   "\t</VideoComponentChain>\n";
            write_motion(motion);
        }

        out += obj("SubClip",sub,"e0c58dc9-dbdd-4166-aef7-5db7e3f22e84",6);
        out += ref(2,"Clip",clip);
        out += uref(2,"MasterClip",m.master_uid);
        out += val(2,"Name",xml_escape(base_name(cap.path)));
        out += "\t\t<OrigChGrp>0</OrigChGrp>\n";
        out += "\t</SubClip>\n";
        write_clip(audio,clip,audio ? m.audio_source : m.video_source,m.markers,in,in + (c.length * cticks),false,1);
        return item;
    }

    // the components of a track of audio, or of the mix of them: its volume and its meter
    void write_track_components(unsigned int chain,int64_t aticks) {
        const unsigned int fader = id(),meter = id(),vol = id(),mute = id();
        out += obj("AudioComponentChain",chain,"3cb131d1-d3c0-47ae-a19a-bdf75ea11674",4);
        out += "\t\t<ComponentChain Version=\"3\">\n"
               "\t\t\t<Components Version=\"1\">\n"
               "\t\t\t\t<Component Index=\"0\" ObjectRef=\"" + num(fader) + "\"/>\n"
               "\t\t\t\t<Component Index=\"1\" ObjectRef=\"" + num(meter) + "\"/>\n"
               "\t\t\t</Components>\n"
               "\t\t</ComponentChain>\n";
        out += val(2,"AudioChannelLayout",STEREO_LAYOUT);
        out += "\t\t<ChannelType>1</ChannelType>\n\t</AudioComponentChain>\n";

        out += obj("AudioFader",fader,"1a38c583-ed5c-11d5-abc4-c1cbf61ec590",3);
        out += "\t\t<AudioComponent Version=\"3\">\n"
               "\t\t\t<Component Version=\"7\">\n"
               "\t\t\t\t<Params Version=\"1\">\n"
               "\t\t\t\t\t<Param Index=\"0\" ObjectRef=\"" + num(vol) + "\"/>\n"
               "\t\t\t\t\t<Param Index=\"1\" ObjectRef=\"" + num(mute) + "\"/>\n"
               "\t\t\t\t</Params>\n"
               "\t\t\t\t<ID>1</ID>\n"
               "\t\t\t</Component>\n";
        out += val(3,"FrameRate",num(aticks)) + val(3,"AudioChannelLayout",STEREO_LAYOUT);
        out += "\t\t\t<ChannelType>1</ChannelType>\n"
               "\t\t\t<AudioComponentType>1</AudioComponentType>\n"
               "\t\t</AudioComponent>\n"
               "\t</AudioFader>\n";
        out += obj("AudioComponentParam",vol,"a714635e-a628-4b27-9d59-77eba47dbc1a",10);
        out += "\t\t<Name>Volume</Name>\n"
               "\t\t<UpperBound>5.6234130859375</UpperBound>\n"
               "\t\t<RangeLocked>false</RangeLocked>\n"
               "\t\t<UnitsString>dB</UnitsString>\n"
               "\t</AudioComponentParam>\n";
        out += obj("AudioComponentParam",mute,"32657501-3aa4-445f-a49b-d09ecb9fa1ae",10);
        out += "\t\t<Name>Mute</Name>\n"
               "\t\t<RangeLocked>false</RangeLocked>\n"
               "\t</AudioComponentParam>\n";

        out += obj("AudioMeter",meter,"72ea4700-f615-11d5-abc4-c186585e63e0",2);
        out += "\t\t<AudioComponent Version=\"3\">\n"
               "\t\t\t<Component Version=\"7\">\n"
               "\t\t\t\t<ID>2</ID>\n"
               "\t\t\t</Component>\n";
        out += val(3,"FrameRate",num(aticks)) + val(3,"AudioChannelLayout",STEREO_LAYOUT);
        out += "\t\t\t<ChannelType>1</ChannelType>\n"
               "\t\t\t<AudioComponentType>2</AudioComponentType>\n"
               "\t\t</AudioComponent>\n"
               "\t</AudioMeter>\n";
    }

    void write_pan(unsigned int pan,bool mix,int64_t aticks) {
        if (mix) {
            out += obj("DefaultPanProcessor",pan,"33a94282-ee2c-11d5-abc4-c1cd7f9e3c10",2);
            out += "\t\t<PanProcessor Version=\"3\">\n"
                   "\t\t\t<AudioComponent Version=\"3\">\n"
                   "\t\t\t\t<Component Version=\"7\">\n"
                   "\t\t\t\t\t<ID>4294967280</ID>\n"
                   "\t\t\t\t</Component>\n";
            out += val(4,"FrameRate",num(aticks)) + val(4,"AudioChannelLayout",STEREO_LAYOUT);
            out += "\t\t\t\t<ChannelType>1</ChannelType>\n"
                   "\t\t\t\t<AudioComponentType>0</AudioComponentType>\n"
                   "\t\t\t</AudioComponent>\n"
                   "\t\t</PanProcessor>\n"
                   "\t\t<DefaultPannerInputChannelType>1</DefaultPannerInputChannelType>\n"
                   "\t\t<DefaultPannerOutputChannelType>1</DefaultPannerOutputChannelType>\n"
                   "\t</DefaultPanProcessor>\n";
            return;
        }
        const unsigned int bal = id();
        out += obj("StereoToStereoPanProcessor",pan,"7bf86a01-efbe-11d5-abc4-c1ce2b1e9090",1);
        out += "\t\t<PanProcessor Version=\"3\">\n"
               "\t\t\t<AudioComponent Version=\"3\">\n"
               "\t\t\t\t<Component Version=\"7\">\n"
               "\t\t\t\t\t<Params Version=\"1\">\n"
               "\t\t\t\t\t\t<Param Index=\"0\" ObjectRef=\"" + num(bal) + "\"/>\n"
               "\t\t\t\t\t</Params>\n"
               "\t\t\t\t\t<ID>4294967280</ID>\n"
               "\t\t\t\t</Component>\n";
        out += val(4,"FrameRate",num(aticks)) + val(4,"AudioChannelLayout",STEREO_LAYOUT);
        out += "\t\t\t\t<ChannelType>1</ChannelType>\n"
               "\t\t\t\t<AudioComponentType>0</AudioComponentType>\n"
               "\t\t\t</AudioComponent>\n"
               "\t\t</PanProcessor>\n"
               "\t</StereoToStereoPanProcessor>\n";
        out += obj("AudioComponentParam",bal,"a714635e-a628-4b27-9d59-77eba47dbc1a",10);
        out += val(2,"StartKeyframe",std::string(START_KEYFRAME) + ",0.5,0,0,0,0,0,0");
        out += "\t\t<CurrentValue>0.5</CurrentValue>\n"
               "\t\t<Name>Balance</Name>\n"
               "\t\t<IsInverted>true</IsInverted>\n"
               "\t</AudioComponentParam>\n";
    }

    void write_sequence(const std::string &seq_uid,const std::vector<media_t> &media) {
        const int64_t fticks = ticks_per(p.rate_num,p.rate_den);
        const int64_t aticks = ticks_per(48000,1);
        const unsigned int vgroup = id(),agroup = id(),dgroup = id(),vchain = id(),mix = id();
        std::vector<std::string> vtracks,atracks;
        for (size_t i=0;i < p.video.size();i++)
            vtracks.push_back(uuid());
        for (size_t i=0;i < p.audio.size();i++)
            atracks.push_back(uuid());

        out += uobj("Sequence",seq_uid,"6a15d903-8739-11d5-af2d-9b7855ad8974",12);
        out += "\t\t<Node Version=\"1\">\n"
               "\t\t\t<Properties Version=\"1\">\n"
               "\t\t\t\t<MZ.WorkInPoint>0</MZ.WorkInPoint>\n";
        out += val(4,"MZ.WorkOutPoint",num(p.frames * fticks));
        out += "\t\t\t\t<MZ.EditLine>0</MZ.EditLine>\n";
        if (p.rate_num == 60000 && p.rate_den == 1001)
            out += "\t\t\t\t<MZ.Sequence.VideoTimeDisplayFormat>107</MZ.Sequence.VideoTimeDisplayFormat>\n";
        out += "\t\t\t\t<MZ.Sequence.AudioTimeDisplayFormat>200</MZ.Sequence.AudioTimeDisplayFormat>\n"
               "\t\t\t\t<MZ.Sequence.EditingModeGUID>9678af98-a7b7-4bdb-b477-7ac9c8df4a4e</MZ.Sequence.EditingModeGUID>\n"
               "\t\t\t\t<MZ.Sequence.PreviewUseMaxBitDepth>false</MZ.Sequence.PreviewUseMaxBitDepth>\n"
               "\t\t\t\t<MZ.Sequence.PreviewUseMaxRenderQuality>false</MZ.Sequence.PreviewUseMaxRenderQuality>\n"
               "\t\t\t\t<MZ.Sequence.PreviewRenderingPresetPath>EncoderPresets\\SequencePreview\\9678af98-a7b7-4bdb-b477-7ac9c8df4a4e\\QuickTime.epr</MZ.Sequence.PreviewRenderingPresetPath>\n"
               "\t\t\t\t<MZ.Sequence.PreviewRenderingPresetCodec>1634755443</MZ.Sequence.PreviewRenderingPresetCodec>\n"
               "\t\t\t\t<MZ.Sequence.PreviewRenderingClassID>1061109567</MZ.Sequence.PreviewRenderingClassID>\n";
        out += val(4,"MZ.Sequence.PreviewFrameSizeWidth",num(p.width));
        out += val(4,"MZ.Sequence.PreviewFrameSizeHeight",num(p.height));
        out += "\t\t\t</Properties>\n"
               "\t\t</Node>\n"
               "\t\t<PersistentGroupContainer Version=\"1\">\n"
               "\t\t\t<LinkContainer Version=\"1\">\n"
               "\t\t\t</LinkContainer>\n"
               "\t\t</PersistentGroupContainer>\n"
               "\t\t<TrackGroups Version=\"1\">\n";
        const char *types[3] = { MEDIA_VIDEO,MEDIA_AUDIO,MEDIA_DATA };
        const unsigned int groups[3] = { vgroup,agroup,dgroup };
        for (int i=0;i < 3;i++) {
            out += "\t\t\t<TrackGroup Version=\"1\" Index=\"" + num(i) + "\">\n";
            out += val(4,"First",types[i]) + ref(4,"Second",groups[i]);
            out += "\t\t\t</TrackGroup>\n";
        }
        out += "\t\t</TrackGroups>\n";
        out += val(2,"Name",xml_escape(p.name));
        out += "\t</Sequence>\n";

        out += obj("VideoTrackGroup",vgroup,"9e9abf7a-0918-49c2-91ae-991b5dde77bb",13);
        out += "\t\t<TrackGroup Version=\"1\">\n\t\t\t<Tracks Version=\"1\">\n";
        for (size_t i=0;i < vtracks.size();i++)
            out += "\t\t\t\t<Track Index=\"" + num(i) + "\" ObjectURef=\"" + vtracks[i] + "\"/>\n";
        out += "\t\t\t</Tracks>\n";
        out += val(3,"FrameRate",num(fticks)) + val(3,"NextTrackID",num(vtracks.size() + 1u));
        out += "\t\t</TrackGroup>\n";
        out += "\t\t<ColorManagementSettings>{&quot;autoToneMapEnabled&quot;:true,&quot;enableLogColorManagement&quot;:2,&quot;graphicsWhiteLuminance&quot;:203,&quot;lutInterpolationMethod&quot;:1}</ColorManagementSettings>\n"
               "\t\t<OutputColorSpace>{&quot;baseColorProfile&quot;:{&quot;colorProfileData&quot;:&quot;AQAAAGQAAAA=&quot;,&quot;colorProfileName&quot;:&quot;BT.709,8-bit,Display-Referred&quot;},&quot;baseProfileType&quot;:1,&quot;colorSpaceMetadata&quot;:{&quot;peakLuminance&quot;:100}}</OutputColorSpace>\n"
               "\t\t<ToneMappingDesaturation>0.5</ToneMappingDesaturation>\n"
               "\t\t<AutoInputGamutCompressionEnabled>true</AutoInputGamutCompressionEnabled>\n"
               "\t\t<IsGraphicsWhiteSameAsProject>false</IsGraphicsWhiteSameAsProject>\n"
               "\t\t<IsColorAwareEffectsEnabledSameAsProject>false</IsColorAwareEffectsEnabledSameAsProject>\n";
        out += val(2,"FrameRect","0,0," + num(p.width) + "," + num(p.height));
        out += "\t\t<ComponentOwner Version=\"1\">\n" + ref(3,"Components",vchain) + "\t\t</ComponentOwner>\n";
        out += "\t</VideoTrackGroup>\n";
        out += obj("VideoComponentChain",vchain,"0970e08a-f58f-4108-b29a-1a717b8e12e2",3);
        out += "\t\t<ComponentChain Version=\"3\">\n\t\t</ComponentChain>\n\t</VideoComponentChain>\n";

        out += obj("AudioTrackGroup",agroup,"9b9238b9-53a8-4cc3-b03f-b36246d052e6",6);
        out += "\t\t<TrackGroup Version=\"1\">\n\t\t\t<Tracks Version=\"1\">\n";
        for (size_t i=0;i < atracks.size();i++)
            out += "\t\t\t\t<Track Index=\"" + num(i) + "\" ObjectURef=\"" + atracks[i] + "\"/>\n";
        out += "\t\t\t</Tracks>\n";
        out += val(3,"FrameRate",num(aticks)) + val(3,"NextTrackID",num(atracks.size() + 2u));
        out += "\t\t</TrackGroup>\n";
        out += ref(2,"MasterTrack",mix);
        out += val(2,"ID",uuid());
        out += "\t\t<AutomationSafeFlags>0</AutomationSafeFlags>\n"
               "\t\t<NumAdaptiveChannels>2</NumAdaptiveChannels>\n"
               "\t</AudioTrackGroup>\n";

        out += obj("DataTrackGroup",dgroup,"b714b71d-6838-48dd-9b77-db19088ced7e",1);
        out += "\t\t<TrackGroup Version=\"1\">\n";
        out += val(3,"FrameRate",num(fticks));
        out += "\t\t\t<NextTrackID>1</NextTrackID>\n\t\t</TrackGroup>\n\t</DataTrackGroup>\n";

        // the video tracks, and their clips
        for (size_t t=0;t < p.video.size();t++) {
            std::vector<unsigned int> items;
            std::string head = uobj("VideoClipTrack",vtracks[t],"f68dcd81-8805-11d5-af2d-9bfa89d4ddd4",1);
            for (const clip_t &c : p.video[t].clips)
                items.push_back(write_track_item(false,c,media,fticks));
            out += head;
            out += "\t\t<ClipTrack Version=\"2\">\n";
            out += track_head((unsigned int)t + 1u,MEDIA_VIDEO,(unsigned int)t,false);
            out += track_items(items,MEDIA_VIDEO,(unsigned int)t);
            out += "\t\t</ClipTrack>\n\t</VideoClipTrack>\n";
        }

        // the audio tracks, their clips, and the mix of them
        for (size_t t=0;t < p.audio.size();t++) {
            std::vector<unsigned int> items;
            for (const clip_t &c : p.audio[t].clips) {
                if (media[c.cap].audio)
                    items.push_back(write_track_item(true,c,media,fticks));
            }
            const unsigned int chain = id(),pan = id();
            out += uobj("AudioClipTrack",atracks[t],"097f6203-99ae-11d5-84f2-8cf14bde7040",7);
            out += "\t\t<ClipTrack Version=\"2\">\n";
            out += track_head((unsigned int)t + 2u,MEDIA_AUDIO,(unsigned int)t,true);
            out += track_items(items,MEDIA_AUDIO,(unsigned int)t);
            out += "\t\t</ClipTrack>\n"
                   "\t\t<AudioTrack Version=\"12\">\n"
                   "\t\t\t<ComponentOwner Version=\"1\">\n" + ref(4,"Components",chain) + "\t\t\t</ComponentOwner>\n";
            out += ref(3,"Panner",pan);
            out += val(3,"ID",uuid());
            out += "\t\t\t<NextPannerID>4294967279</NextPannerID>\n"
                   "\t\t</AudioTrack>\n"
                   "\t</AudioClipTrack>\n";
            write_track_components(chain,aticks);
            write_pan(pan,false,aticks);
        }

        {
            const unsigned int chain = id(),pan = id(),inlet = id();
            out += obj("AudioMixTrack",mix,"4b1d8400-e89e-11d5-abc4-a1a13b1e80a0",4);
            out += "\t\t<AudioTrack Version=\"12\">\n"
                   "\t\t\t<ComponentOwner Version=\"1\">\n" + ref(4,"Components",chain) + "\t\t\t</ComponentOwner>\n";
            out += ref(3,"Panner",pan);
            out += val(3,"ID",uuid());
            out += "\t\t\t<SubType>3</SubType>\n"
                   "\t\t\t<Assign>0</Assign>\n"
                   "\t\t\t<NextPannerID>4294967279</NextPannerID>\n"
                   "\t\t</AudioTrack>\n"
                   "\t\t<Track Version=\"4\">\n"
                   "\t\t\t<Node Version=\"1\">\n"
                   "\t\t\t\t<Properties Version=\"1\">\n"
                   "\t\t\t\t\t<TL.SQTrackExpanded>0</TL.SQTrackExpanded>\n"
                   "\t\t\t\t\t<TL.SQTrackExpandedHeight>17</TL.SQTrackExpandedHeight>\n"
                   "\t\t\t\t\t<TL.SQTrackShy>0</TL.SQTrackShy>\n"
                   "\t\t\t\t\t<TL.SQTrackAudioKeyframeStyle>2</TL.SQTrackAudioKeyframeStyle>\n"
                   "\t\t\t\t</Properties>\n"
                   "\t\t\t</Node>\n"
                   "\t\t\t<ID>1</ID>\n";
            out += val(3,"MediaType",MEDIA_AUDIO);
            out += "\t\t\t<Index>0</Index>\n"
                   "\t\t</Track>\n";
            out += ref(2,"Inlet",inlet);
            out += "\t</AudioMixTrack>\n";
            write_track_components(chain,aticks);
            write_pan(pan,true,aticks);
            out += obj("AudioTrackInlet",inlet,"be3af080-e8c6-11d5-abc4-a1c6d5dee670",4);
            out += "\t\t<Sources Version=\"1\">\n";
            for (size_t i=0;i < atracks.size();i++)
                out += "\t\t\t<Source Index=\"" + num(i) + "\" ObjectURef=\"" + atracks[i] + "\"/>\n";
            out += "\t\t</Sources>\n";
            out += val(2,"AudioChannelLayout",STEREO_LAYOUT);
            out += "\t</AudioTrackInlet>\n";
        }
    }

    const project_t&            p;
    const std::vector<capture_t>& caps;
    std::string                 path;
    std::string                 out;
    unsigned int                next_id = PRPROJ_FIRST_ID;
    std::mt19937_64             rng;
};

bool write_prproj(const std::string &path,const project_t &p) {
    prproj_writer_t w(p,path);
    const std::string xml = w.write();

    gzFile gz = gzopen(path.c_str(),"wb6");
    if (gz == NULL) {
        fprintf(stderr,"%s: cannot write it\n",path.c_str());
        return false;
    }
    bool ok = true;
    for (size_t i=0;i < xml.size() && ok;) {
        const unsigned int n = (unsigned int)std::min((size_t)(1u << 20),xml.size() - i);
        ok = gzwrite(gz,xml.data() + i,n) == (int)n;
        i += n;
    }
    if (gzclose(gz) != Z_OK)
        ok = false;
    if (!ok)
        fprintf(stderr,"%s: error writing it\n",path.c_str());
    return ok;
}

/////////////////////////////////////////////////////////////////////////////
// Final Cut Pro 7 XML

static std::string url_path(const std::string &path) {
    std::string r = "file://localhost";
    if (path.empty() || path[0] != '/')
        r += '/';
    for (unsigned char c : path) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            r += (char)c;
        }
        else if (c == '\\') {
            r += '/';
        }
        else {
            char buf[8];
            snprintf(buf,sizeof(buf),"%%%02x",c);
            r += buf;
        }
    }
    return r;
}

// the rate of a sequence or clip, as xmeml has it: a whole number of frames a second, and whether it is
// 1000/1001 of that
static std::string fcp_rate(int num,int den,int tabs) {
    const bool ntsc = den == 1001;
    const long tb = ntsc ? lround((double)num / 1000.0) : lround((double)num / (double)den);
    const std::string t((size_t)tabs,'\t');
    return t + "<rate>\n" + t + "\t<timebase>" + std::to_string(tb) + "</timebase>\n" + t + "\t<ntsc>" + (ntsc ? "TRUE" : "FALSE") + "</ntsc>\n" + t + "</rate>\n";
}

static const char *fcp_par(const capture_t &c) {
    if (c.sar_num == c.sar_den)
        return "square";
    const double sar = (double)c.sar_num / (double)c.sar_den;
    if (c.height == 480 || c.height == 486)
        return sar > 1.0 ? "NTSC-CCIR-601" : "NTSC-601";
    if (c.height == 576)
        return sar > 1.2 ? "PAL-CCIR-601" : "PAL-601";
    return "square";
}

bool write_fcpxml(const std::string &path,const project_t &p) {
    const std::vector<capture_t> &caps = *p.caps;
    std::string x;
    std::vector<bool> file_done(caps.size(),false);
    unsigned int clipitem = 1;

    auto file_elem = [&](unsigned int cap,int tabs) {
        const capture_t &c = caps[cap];
        const std::string t((size_t)tabs,'\t');
        const std::string fid = "file-" + std::to_string(cap + 1u);
        if (file_done[cap]) {
            x += t + "<file id=\"" + fid + "\"/>\n";
            return;
        }
        file_done[cap] = true;
        x += t + "<file id=\"" + fid + "\">\n";
        x += t + "\t<name>" + xml_escape(base_name(c.path)) + "</name>\n";
        x += t + "\t<pathurl>" + xml_escape(url_path(map_path(absolute(c.path),p))) + "</pathurl>\n";
        x += fcp_rate(c.rate_num,c.rate_den,tabs + 1);
        x += t + "\t<duration>" + std::to_string(c.position(c.frames() - 1u) + 1) + "</duration>\n";
        x += t + "\t<media>\n";
        x += t + "\t\t<video>\n" + t + "\t\t\t<samplecharacteristics>\n";
        x += fcp_rate(c.rate_num,c.rate_den,tabs + 4);
        x += t + "\t\t\t\t<width>" + std::to_string(c.width) + "</width>\n";
        x += t + "\t\t\t\t<height>" + std::to_string(c.height) + "</height>\n";
        x += t + "\t\t\t\t<anamorphic>FALSE</anamorphic>\n";
        x += t + "\t\t\t\t<pixelaspectratio>" + fcp_par(c) + "</pixelaspectratio>\n";
        x += t + "\t\t\t\t<fielddominance>none</fielddominance>\n";
        x += t + "\t\t\t</samplecharacteristics>\n" + t + "\t\t</video>\n";
        if (c.audio_rate > 0) {
            x += t + "\t\t<audio>\n" + t + "\t\t\t<samplecharacteristics>\n";
            x += t + "\t\t\t\t<depth>16</depth>\n";
            x += t + "\t\t\t\t<samplerate>" + std::to_string(c.audio_rate) + "</samplerate>\n";
            x += t + "\t\t\t</samplecharacteristics>\n";
            x += t + "\t\t\t<channelcount>" + std::to_string(c.audio_channels) + "</channelcount>\n";
            x += t + "\t\t</audio>\n";
        }
        x += t + "\t</media>\n";
        x += t + "</file>\n";
    };

    auto clip_elem = [&](const clip_t &cl,bool audio) {
        const capture_t &c = caps[cl.cap];
        const int64_t in = c.position((size_t)cl.frame);
        x += std::string("\t\t\t\t\t<clipitem id=\"clipitem-") + std::to_string(clipitem++) + "\"" + (audio ? " premiereChannelType=\"stereo\"" : "") + ">\n";
        x += "\t\t\t\t\t\t<name>" + xml_escape(base_name(c.path)) + "</name>\n";
        x += "\t\t\t\t\t\t<enabled>TRUE</enabled>\n";
        x += "\t\t\t\t\t\t<duration>" + std::to_string(c.position(c.frames() - 1u) + 1) + "</duration>\n";
        x += fcp_rate(c.rate_num,c.rate_den,6);
        x += "\t\t\t\t\t\t<start>" + std::to_string(cl.start) + "</start>\n";
        x += "\t\t\t\t\t\t<end>" + std::to_string(cl.start + cl.length) + "</end>\n";
        x += "\t\t\t\t\t\t<in>" + std::to_string(in) + "</in>\n";
        x += "\t\t\t\t\t\t<out>" + std::to_string(in + cl.length) + "</out>\n";
        file_elem(cl.cap,6);
        if (audio)
            x += "\t\t\t\t\t\t<sourcetrack>\n"
                 "\t\t\t\t\t\t\t<mediatype>audio</mediatype>\n"
                 "\t\t\t\t\t\t\t<trackindex>1</trackindex>\n"
                 "\t\t\t\t\t\t</sourcetrack>\n";
        x += "\t\t\t\t\t</clipitem>\n";
    };

    x = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE xmeml>\n<xmeml version=\"4\">\n";
    x += "\t<sequence id=\"sequence-1\">\n";
    x += "\t\t<name>" + xml_escape(p.name) + "</name>\n";
    x += "\t\t<duration>" + std::to_string(p.frames) + "</duration>\n";
    x += fcp_rate(p.rate_num,p.rate_den,2);
    x += "\t\t<media>\n";
    x += "\t\t\t<video>\n";
    x += "\t\t\t\t<format>\n\t\t\t\t\t<samplecharacteristics>\n";
    x += fcp_rate(p.rate_num,p.rate_den,6);
    x += "\t\t\t\t\t\t<width>" + std::to_string(p.width) + "</width>\n";
    x += "\t\t\t\t\t\t<height>" + std::to_string(p.height) + "</height>\n";
    x += "\t\t\t\t\t\t<anamorphic>FALSE</anamorphic>\n";
    x += "\t\t\t\t\t\t<pixelaspectratio>square</pixelaspectratio>\n";
    x += "\t\t\t\t\t\t<fielddominance>none</fielddominance>\n";
    x += "\t\t\t\t\t</samplecharacteristics>\n\t\t\t\t</format>\n";
    for (const track_t &t : p.video) {
        x += "\t\t\t\t<track>\n";
        for (const clip_t &cl : t.clips)
            clip_elem(cl,false);
        x += "\t\t\t\t\t<enabled>TRUE</enabled>\n";
        x += "\t\t\t\t</track>\n";
    }
    x += "\t\t\t</video>\n";
    x += "\t\t\t<audio>\n";
    x += "\t\t\t\t<numOutputChannels>2</numOutputChannels>\n";
    for (const track_t &t : p.audio) {
        x += "\t\t\t\t<track premiereTrackType=\"Stereo\">\n";
        for (const clip_t &cl : t.clips) {
            if (caps[cl.cap].audio_rate > 0)
                clip_elem(cl,true);
        }
        x += "\t\t\t\t\t<enabled>TRUE</enabled>\n";
        x += "\t\t\t\t\t<outputchannelindex>1</outputchannelindex>\n";
        x += "\t\t\t\t</track>\n";
    }
    x += "\t\t\t</audio>\n";
    x += "\t\t</media>\n";
    x += "\t</sequence>\n";
    x += "</xmeml>\n";

    FILE *fp = fopen(path.c_str(),"wb");
    if (fp == NULL) {
        fprintf(stderr,"%s: cannot write it\n",path.c_str());
        return false;
    }
    bool ok = fwrite(x.data(),1,x.size(),fp) == x.size();
    if (fclose(fp) != 0)
        ok = false;
    if (!ok)
        fprintf(stderr,"%s: error writing it\n",path.c_str());
    return ok;
}
