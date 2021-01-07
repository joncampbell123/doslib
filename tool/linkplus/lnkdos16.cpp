
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

extern "C" {
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>
}

using namespace std;

#include <algorithm>
#include <memory>
#include <vector>
#include <string>

#if defined(_MSC_VER)
# define strcasecmp strcmpi
#endif

#ifndef O_BINARY
#define O_BINARY (0)
#endif

enum {
    PASS_GATHER,
    PASS_BUILD,
    PASS_MAX
};

enum {
    OFMTVAR_NONE=0,

    OFMTVAR_COMREL=10
};

enum {
    OFMT_COM=0,
    OFMT_EXE,
    OFMT_DOSDRV,
    OFMT_DOSDRVEXE
};

//================================== PROGRAM ================================

/* <file ref> <module index ref>
 *
 * for .obj files, file ref is an index and module index ref is zero.
 * for .lib files, file ref is an index and module index is an index into the embedded .obj files within */

typedef size_t                          in_fileRef;             /* index into in_file */
typedef uint32_t                        in_fileModuleRef;       /* within a file ref */
typedef uint32_t                        segmentSize;            /* segment size */
typedef uint32_t                        segmentBase;            /* segment base value */
typedef uint32_t                        segmentOffset;          /* offset from segment */
typedef uint32_t                        segmentRelative;        /* segment relative to base */
typedef uint32_t                        alignMask;              /* alignment mask for data alignment. ~0u (all 1s) means byte alignment. Must be inverse of power of 2 */
typedef uint32_t                        fileOffset;
typedef uint32_t                        linearAddress;
typedef size_t                          segmentRef;
typedef size_t                          segmentIndex;

static const in_fileRef                 in_fileRefUndef = ~((in_fileRef)0u);
static const in_fileRef                 in_fileRefInternal = in_fileRefUndef - (in_fileRef)1u;
static const in_fileRef                 in_fileRefPadding = in_fileRefUndef - (in_fileRef)2u;
static const in_fileModuleRef           in_fileModuleRefUndef = ~((in_fileModuleRef)0u);
static const segmentIndex               segmentIndexUndef = ~((segmentIndex)0u);
static const segmentSize                segmentSizeUndef = ~((segmentSize)0u);
static const segmentBase                segmentBaseUndef = ~((segmentBase)0u);
static const segmentOffset              segmentOffsetUndef = ~((segmentOffset)0u);
static const alignMask                  byteAlignMask = ~((alignMask)0u);
static const alignMask                  wordAlignMask = ~((alignMask)1u);
static const alignMask                  dwordAlignMask = ~((alignMask)3u);
static const alignMask                  qwordAlignMask = ~((alignMask)7u);
static const fileOffset                 fileOffsetUndef = ~((fileOffset)0u);
static const linearAddress              linearAddressUndef = ~((linearAddress)0u);

static inline alignMask alignMaskToValue(const alignMask &v) {
    return (~v) + ((alignMask)1u);
}

static inline alignMask alignValueToAlignMask(const alignMask &v) {
    return (~v) + ((alignMask)1u);
}

static FILE*                            map_fp = NULL;

struct input_file {
    string                              path;
    int                                 segment_group;

    input_file() : segment_group(-1) { }
};

struct cmdoptions {
    unsigned int                        do_dosseg:1;
    unsigned int                        verbose:1;
    unsigned int                        prefer_flat:1;

    unsigned int                        output_format;
    unsigned int                        output_format_variant;

    int                                 next_segment_group;

    segmentSize                         want_stack_size;

    segmentBase                         image_base_segment_reloc_adjust;/* segment value to add to relocation entries in order to work with relocation fixup code (COMREL) */
    segmentBase                         image_base_segment;             /* base segment of executable image. For .COM images, this is -0x10 aka 0x...FFF0 */
    segmentOffset                       image_base_offset;              /* base offset, relative to base segment, of executable image */

    string                              dosdrv_header_symbol;
    string                              hex_output;
    string                              out_file;
    string                              map_file;

    vector<input_file>                  in_file;

    cmdoptions() : do_dosseg(true), verbose(false), prefer_flat(false), output_format(OFMT_COM),
                   output_format_variant(OFMTVAR_NONE), next_segment_group(-1), want_stack_size(4096),
                   image_base_segment_reloc_adjust(segmentBaseUndef), image_base_segment(segmentBaseUndef),
                   image_base_offset(segmentOffsetUndef), dosdrv_header_symbol("_dosdrv_header") { }
};

static cmdoptions                       cmdoptions;

static in_fileRef                       current_in_file = 0;
static in_fileModuleRef                 current_in_file_module = 0;
static int                              current_segment_group = -1;

const char *get_in_file(const in_fileRef idx) {
    if (idx == in_fileRefUndef)
        return "<undefined>";
    else if (idx == in_fileRefInternal)
        return "<internal>";
    else if (idx == in_fileRefPadding)
        return "<padding>";
    else if (idx < cmdoptions.in_file.size()) {
        if (!cmdoptions.in_file[idx].path.empty())
            return cmdoptions.in_file[idx].path.c_str();
        else
            return "<noname>";
    }

    return "<outofrange>";
}

struct omf_context_t*                   omf_state = NULL;

/* comrel entry point */
#define comrel_entry_point_CX_COUNT         0x04
#define comrel_entry_point_SI_OFFSET        0x07
#define comrel_entry_point_JMP_ENTRY        0x11
static const uint8_t comrel_entry_point[] = {
    0xFC,                               // 0x00 CLD
    0x8C,0xCA,                          // 0x01 MOV DX,CS
    0xB9,0x00,0x00,                     // 0x03 MOV CX,<count>
    0xBE,0x00,0x00,                     // 0x06 MOV SI,<table offset>
    0xAD,                               // 0x09 <loop1>  LODSW
    0x89,0xC3,                          // 0x0A MOV BX,AX
    0x01,0x17,                          // 0x0C ADD [BX],DX
    0xE2,0x100u-7u,                     // 0x0E LOOP <loop1>
    0xE9,0x00,0x00                      // 0x10 JMP rel <target>
                                        // 0x13
};

//#define dosdrvrel_entry_debug

/* dosdrvrel entry point */
#ifdef dosdrvrel_entry_debug
# define dosdrvrel_entry_debug_O 1
#else
# define dosdrvrel_entry_debug_O 0
#endif

#define dosdrvrel_entry_point_entry1           (0x01+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_code_intr        (0x05+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_entry2           (0x06+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_CX_COUNT         (0x0C+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_SI_OFFSET        (0x0F+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_orig_entry1      (0x1F+dosdrvrel_entry_debug_O)
#define dosdrvrel_entry_point_orig_entry2      (0x24+dosdrvrel_entry_debug_O)
static const uint8_t dosdrvrel_entry_point[] = {
#ifdef dosdrvrel_entry_debug
    0xCC,                               //
#endif
    0xBF,0x00,0x00,                     // 0x00 MOV DI,<offset>
    0xEB,0x03,                          // 0x03 JMP short (entry)

    0xBF,0x00,0x00,                     // 0x05 MOV DI,<offset>

    0xFC,                               // 0x08 CLD (entry)
    0x8C,0xCA,                          // 0x09 MOV DX,CS
    0xB9,0x00,0x00,                     // 0x0B MOV CX,<count>
    0xBE,0x00,0x00,                     // 0x0E MOV SI,<table offset>
    0x53,                               // 0x11 PUSH BX
    0xAD,                               // 0x12 <loop1>  LODSW
    0x89,0xC3,                          // 0x13 MOV BX,AX
    0x01,0x17,                          // 0x15 ADD [BX],DX
    0xE2,0x100u-7u,                     // 0x17 LOOP <loop1>
    0x5B,                               // 0x19 POP BX
    0xBE,0x06,0x00,                     // 0x1A MOV SI,0x0006
    0xC7,0x04,0x00,0x00,                // 0x1D MOV [SI],<original value>
    0xC7,0x44,0x02,0x00,0x00,           // 0x21 MOV [SI+2],<original value>
    0xFF,0xE7                           // 0x26 JMP DI
                                        // 0x28
};

struct seg_fragment; // forward decl
struct link_segdef;

typedef shared_ptr<struct seg_fragment> fragmentRef;
static const fragmentRef                fragmentRefUndef = shared_ptr<struct seg_fragment>(nullptr);

struct exe_relocation {
    shared_ptr<struct link_segdef>      segref;             /* which segment */
    fragmentRef                         fragment;           /* which fragment */
    segmentOffset                       offset;             /* offset within fragment */

    exe_relocation() : fragment(fragmentRefUndef), offset(segmentOffsetUndef) { }
};

/* array of EXE relocation entries (memory locations referring to a segment that need adjustment by EXE image base) */
static vector< shared_ptr<struct exe_relocation> >          exe_relocation_table;

shared_ptr<struct exe_relocation> new_exe_relocation(void) {
    shared_ptr<struct exe_relocation> r(new struct exe_relocation);
    exe_relocation_table.push_back(r);
    return r;
}

void free_exe_relocations(void) {
    exe_relocation_table.clear();
}

struct link_symbol {
    string                              name;               /* symbol name, raw */
    shared_ptr<struct link_segdef>      segref;             /* belongs to segdef */
    string                              groupdef;           /* belongs to groupdef (not used except for display) */
    segmentOffset                       offset;             /* offset within fragment */
    fragmentRef                         fragment;           /* which fragment it belongs to */
    in_fileRef                          in_file;            /* from which file */
    in_fileModuleRef                    in_module;          /* from which module */
    unsigned int                        is_local:1;         /* is local symbol */

    link_symbol() : offset(0), fragment(fragmentRefUndef), in_file(in_fileRefUndef), in_module(in_fileModuleRefUndef), is_local(0) { }
};

static vector< shared_ptr<struct link_symbol> >             link_symbols;

shared_ptr<struct link_symbol> new_link_symbol(const char *name) {
    shared_ptr<struct link_symbol> sym(new struct link_symbol);
    link_symbols.push_back( sym );
    sym->name = name;
    return sym;
}

shared_ptr<struct link_symbol> find_link_symbol(const char *name,const in_fileRef in_file,const in_fileModuleRef in_module) {
    for (size_t i=0;i < link_symbols.size();i++) {
        shared_ptr<struct link_symbol> sym = link_symbols[i];

        if (sym->is_local) {
            /* ignore local symbols unless file/module scope is given */
            if (in_file != in_fileRefUndef && sym->in_file != in_file)
                continue;
            if (in_module != in_fileModuleRefUndef && sym->in_module != in_module)
                continue;
        }

        if (sym->name == name)
            return link_symbols[i];
    }

    return NULL;
}

struct seg_fragment {
    in_fileRef                          in_file;            /* fragment comes from file */
    in_fileModuleRef                    in_module;          /* fragment comes from this module index */
    segmentIndex                        from_segment_index; /* from which segment (by index) if applicable for the OBJ file (OMF) */
    segmentOffset                       offset;             /* offset in segment */
    segmentSize                         fragment_length;    /* length of fragment */
    alignMask                           fragment_alignment; /* alignment of fragment */
    struct omf_segdef_attr_t            attr;               /* fragment attributes */

    vector<unsigned char>               image;              /* in memory image of segment during construction */

    seg_fragment() : in_file(in_fileRefUndef), in_module(in_fileModuleRefUndef), from_segment_index(segmentIndexUndef),
                     offset(segmentOffsetUndef), fragment_length(segmentSizeUndef), fragment_alignment(byteAlignMask), attr({0,0,{0}}) { }
};

struct link_segdef {
    struct omf_segdef_attr_t            attr;               /* segment attributes */
    string                              name;               /* name of segment */
    string                              classname;          /* class of segment */
    string                              groupname;          /* group of segment */
    fileOffset                          file_offset;        /* file offset chosen to write segment, undef if not yet chosen */
    linearAddress                       linear_offset;      /* linear offset in memory relative to image base [*2] */
    segmentOffset                       segment_base;       /* base offset in memory of segment (for example, 100h for .COM, 0h for .EXE) [*3] */
    segmentOffset                       segment_offset;     /* offset within segment in memory (with segment_base added in) */
    segmentSize                         segment_length;     /* length in bytes */
    segmentRelative                     segment_relative;   /* segment number relative to image base segment in memory [*1] */
    segmentRelative                     segment_reloc_adj;  /* segment relocation adjustment at FIXUP time */
    alignMask                           segment_alignment;  /* alignment of segment. This is a bitmask. */
    fragmentRef                         fragment_load_index;/* current fragment, used when processing LEDATA and OMF symbols, in both passes */
    vector< shared_ptr<struct seg_fragment> > fragments;    /* fragments (one from each OBJ/module) */

    int                                 segment_group;

    unsigned int                        pinned:1;           /* segment is pinned at it's position in the segment order, should not move */
    unsigned int                        noemit:1;           /* segment will not be written to disk (usually BSS and STACK) */
    unsigned int                        header:1;           /* segment is executable header stuff */

    link_segdef() : attr({0,0,{0}}), file_offset(fileOffsetUndef), linear_offset(linearAddressUndef), segment_base(0), segment_offset(segmentOffsetUndef),
                    segment_length(0), segment_relative(0), segment_reloc_adj(0), segment_alignment(byteAlignMask), fragment_load_index(fragmentRefUndef),
                    segment_group(-1), pinned(0), noemit(0), header(0) { }
};
/* NOTE [*1]: segment_relative has meaning only in segmented modes. In real mode, it is useful in determining where things
 *            are in memory because of the nature of 16-bit real mode, where it is a segment value relative to a base
 *            segment offset (0 for EXE, -0x10 for COM). In protected mode, this is just a segment index because the OS or
 *            loader determines what protected mode selectors we get. In flat (non-segmented) modes, segment_relative has
 *            no meaning. */
/* NOTE [*2]: linear_offset is used in 16-bit real mode to compute the byte offset from the image base (as if segmentation
 *            did not exist) and is used to compute segment_relative in some parts of this code. linear_offset has no
 *            meaning in segmented protected mode. In flat protected mode linear_offset will generally match segment_offset
 *            since all parts of the image are loaded into one segment. They may not match if for any reason we are
 *            linking to some esoteric flat memory model. */
/* NOTE [*3]: segment_base is meant to represent the in-memory base offset of the segment that segment_offset is calculated
 *            from. For example .COM files will have segment_base == 0x100 because DOS loads the .COM image into a segment
 *            0x100 bytes from the start and defines the first 0x100 bytes before the image as the PSP (Program Segment Prefix).
 *            This linker also supports linking .EXE files as if a .COM file translated to .EXE (EXE2BIN) where the
 *            entry point is set up like a .COM executable (this is where you see values like 0xFFF0:0x0100 in the header
 *            that due to 16-bit segmented wraparound, start execution with CS pointing at the PSP segment and the
 *            instruction pointer at the first byte at CS:0x100). All segment_offset values are calculated with segment_base
 *            added in, to simplify the code. That means you can't just change segment_base on a whim. Separate segment_base
 *            per segdef allows different segments to operate differently, though currently all have the same image_base_offset
 *            value. segment_offset will generally be zero in segmented protected mode, and in flat protected mode. */

static vector< shared_ptr<struct link_segdef> > link_segments;

static fragmentRef                      entry_seg_link_target_fragment = fragmentRefUndef;
static shared_ptr<link_segdef>          entry_seg_link_target;
static shared_ptr<link_segdef>          entry_seg_link_frame;
static segmentOffset                    entry_seg_ofs = 0;

/* Open Watcom DOSSEG linker order
 * 
 * 1. not DGROUP, class CODE
 * 2. not DGROUP
 * 3. group DGROUP, class BEGDATA
 * 4. group DGROUP, not (class BEGDATA or class BSS or class STACK)
 * 5. group DGROUP, class BSS
 * 6. group DGROUP, class STACK */

/* 1. return -1 if not DGROUP, class CODE (move up) */
int sort_cmp_not_dgroup_class_code(const struct link_segdef *a) {
    if (a->groupname.empty() || a->groupname == "DGROUP") { /* not DGROUP */
        if (a->classname == "CODE") { /* CODE */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 2. return -1 if not DGROUP */
int sort_cmp_not_dgroup(const struct link_segdef *a) {
    if (a->groupname.empty() || a->groupname == "DGROUP") { /* not DGROUP */
        /* OK */
        return -1;
    }

    return 0;
}

/* 3. return -1 if group DGROUP, class BEGDATA */
int sort_cmp_dgroup_class_BEGDATA(const struct link_segdef *a) {
    if (a->groupname == "DGROUP") { /* DGROUP */
        if (a->classname == "BEGDATA") { /* BEGDATA */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 4. return -1 if group DGROUP, not (class BEGDATA or class BSS or class STACK) */
int sort_cmp_dgroup_class_not_special(const struct link_segdef *a) {
    if (a->groupname == "DGROUP") { /* DGROUP */
        if (a->classname == "BEGDATA" || a->classname == "BSS" || a->classname == "STACK") { /* BEGDATA */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 5. return -1 if group DGROUP, class BSS */
int sort_cmp_dgroup_class_bss(const struct link_segdef *a) {
    if (a->groupname == "DGROUP") { /* DGROUP */
        if (a->classname == "BSS") { /* BSS */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 6. return -1 if group DGROUP, class STACK */
int sort_cmp_dgroup_class_stack(const struct link_segdef *a) {
    if (a->groupname == "DGROUP") { /* DGROUP */
        if (a->classname == "STACK") { /* STACK */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* return 1 if class BSS */
int sort_cmpf_class_bss(const struct link_segdef *a) {
    if (a->classname == "BSS") { /* STACK */
        /* OK */
        return 1;
    }

    return 0;
}

/* return 1 if class STACK */
int sort_cmpf_class_stack(const struct link_segdef *a) {
    if (a->classname == "STACK") { /* STACK */
        /* OK */
        return 1;
    }

    return 0;
}

void link_segments_swap(unsigned int s1,unsigned int s2) {
    if (s1 != s2)
        swap(link_segments[s1],link_segments[s2]);
}

shared_ptr<struct link_segdef> find_link_segment(const char *name);

void link_segments_sort(unsigned int *start,unsigned int *end,int (*sort_cmp)(const struct link_segdef *a)) {
    unsigned int i;
    int r;

    for (i=*start;i <= *end;) {
        r = sort_cmp(link_segments[i].get());
        if (r < 0) {
            while (i > *start) {
                i--;
                link_segments_swap(i,i+1);
            }

            (*start)++;
            i++;
        }
        else if (r > 0) {
            while (i < *end) {
                link_segments_swap(i,i+1);
                i++;
            }

            (*end)--;
            i--;
        }
        else {
            i++;
        }
    }
}

void owlink_dosseg_sort_order(void) {
    /* WARNING: list should have already been sorted by segment groups.
     *          BSS/STACK however is ALWAYS moved to the bottom of the image. */
    for (unsigned int gsi=0;gsi < link_segments.size();gsi++) {
        unsigned int s = gsi,e = s;

        while (e < link_segments.size() && link_segments[e]->segment_group == link_segments[s]->segment_group) e++;
        gsi = e;
        e--;

        link_segments_sort(&s,&e,sort_cmp_not_dgroup_class_code);       /* 1 */
        link_segments_sort(&s,&e,sort_cmp_not_dgroup);                  /* 2 */
        link_segments_sort(&s,&e,sort_cmp_dgroup_class_BEGDATA);        /* 3 */
        link_segments_sort(&s,&e,sort_cmp_dgroup_class_not_special);    /* 4 */
    }

    if (link_segments.size() != 0) {
        unsigned int s = 0,e = link_segments.size() - 1u;

        link_segments_sort(&s,&e,sort_cmp_dgroup_class_bss);            /* 5 */
        link_segments_sort(&s,&e,sort_cmp_dgroup_class_stack);          /* 6 */
    }
}

bool owlink_segsrt_def_qsort_cmp(const shared_ptr<struct link_segdef> &sa, const shared_ptr<struct link_segdef> &sb) {
    /* if either one is pinned, don't move */
    if (sa->pinned || sb->pinned) return false;

    /* sort by segment_group, GROUP, CLASS */

    /* do it */
    if (sa->segment_group < sb->segment_group)
        return true;
    if (sa->segment_group > sb->segment_group)
        return false;

    if (sa->groupname < sb->groupname)
        return true;
    if (sa->groupname > sb->groupname)
        return false;

    if (sa->classname < sb->classname)
        return true;
    if (sa->classname > sb->classname)
        return false;

    return false;
}

void owlink_default_sort_seg(void) {
    sort(link_segments.begin(), link_segments.end(), owlink_segsrt_def_qsort_cmp);
}

void owlink_stack_bss_arrange(void) {
    unsigned int s = 0,e = link_segments.size() - 1u;

    if (link_segments.size() == 0) return;
    if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRV || cmdoptions.output_format == OFMT_DOSDRVEXE) {
        /* STACK and BSS must be placed at the end in BSS, STACK order */
        e = link_segments.size() - 1u;
        s = 0;

        link_segments_sort(&s,&e,sort_cmpf_class_stack);
        link_segments_sort(&s,&e,sort_cmpf_class_bss);
    }
}

shared_ptr<struct seg_fragment> alloc_link_segment_fragment(struct link_segdef *sg) {
    shared_ptr<struct seg_fragment> frag(new struct seg_fragment);
    sg->fragments.push_back(frag);
    return frag;
}

shared_ptr<struct seg_fragment> alloc_link_segment_fragment(struct link_segdef *sg,size_t &index) {
    shared_ptr<struct seg_fragment> frag(new struct seg_fragment);
    index = sg->fragments.size();
    sg->fragments.push_back(frag);
    return frag;
}

void free_link_segments(void) {
    link_segments.clear();
}

unsigned int omf_align_code_to_bytes(const unsigned int x) {
    switch (x) {
        case OMF_SEGDEF_RELOC_BYTE:         return 1;
        case OMF_SEGDEF_RELOC_WORD:         return 2;
        case OMF_SEGDEF_RELOC_PARA:         return 16;
        case OMF_SEGDEF_RELOC_PAGE:         return 4096;
        case OMF_SEGDEF_RELOC_DWORD:        return 4;
        default:                            break;
    };

    return 0;
}

void dump_link_relocations(void) {
    unsigned int i=0;

    if (exe_relocation_table.empty()) return;

    if (map_fp != NULL) {
        fprintf(map_fp,"\n");
        fprintf(map_fp,"Relocation table: %u entries\n",(unsigned int)exe_relocation_table.size());
        fprintf(map_fp,"---------------------------------------\n");
    }

    while (i < exe_relocation_table.size()) {
        shared_ptr<struct exe_relocation> rel = exe_relocation_table[i++];

        if (cmdoptions.verbose) {
            fprintf(stderr,"relocation[%u]: seg='%s' frag=%p offset=0x%lx\n",
                i,rel->segref->name.c_str(),(void*)rel->fragment.get(),(unsigned long)rel->offset);
        }

        if (map_fp != NULL) {
            shared_ptr<struct link_segdef> sg = rel->segref;
            shared_ptr<struct seg_fragment> frag = rel->fragment;

            fprintf(map_fp,"  %04lx:%08lx [0x%08lx] %20s + 0x%08lx from '%s':%u\n",
                sg->segment_relative&0xfffful,
                (unsigned long)sg->segment_offset + (unsigned long)frag->offset + (unsigned long)rel->offset,
                (unsigned long)sg->linear_offset + (unsigned long)frag->offset + (unsigned long)rel->offset,
                rel->segref->name.c_str(),(unsigned long)frag->offset + (unsigned long)rel->offset,
                get_in_file(frag->in_file),frag->in_module);
        }
    }

    if (map_fp != NULL)
        fprintf(map_fp,"\n");
}

bool link_symbol_qsort_cmp_by_name(const shared_ptr<struct link_symbol> &sa,const shared_ptr<struct link_symbol> &sb) {
    return strcasecmp(sa->name.c_str(),sb->name.c_str()) < 0;
}

bool link_symbol_qsort_cmp(const shared_ptr<struct link_symbol> &sa,const shared_ptr<struct link_symbol> &sb) {
    /* -----A----- */
    shared_ptr<struct link_segdef> sga = sa->segref;
    shared_ptr<struct seg_fragment> fraga = sa->fragment;

    /* -----B----- */
    shared_ptr<struct link_segdef> sgb = sb->segref;
    shared_ptr<struct seg_fragment> fragb = sb->fragment;

    segmentRelative la,lb;

    /* segment */
    la = sga->segment_relative;
    lb = sgb->segment_relative;

    if (la < lb) return true;
    if (la > lb) return false;

    /* offset */
    la = sga->segment_offset + fraga->offset + sa->offset;
    lb = sgb->segment_offset + fragb->offset + sb->offset;

    if (la < lb) return true;
    if (la > lb) return false;

    return false;
}

bool link_segments_qsort_frag_by_offset(const shared_ptr<struct seg_fragment> &sa,const shared_ptr<struct seg_fragment> &sb) {
    return sa->offset < sb->offset;
}

bool link_segments_qsort_by_linofs(const shared_ptr<struct link_segdef> &sa,const shared_ptr<struct link_segdef> &sb) {
    return sa->linear_offset < sb->linear_offset;
}

bool link_segments_qsort_by_fileofs(const shared_ptr<struct link_segdef> &sa,const shared_ptr<struct link_segdef> &sb) {
    return sa->file_offset < sb->file_offset;
}

void dump_link_symbols(void) {
    unsigned int i,pass=0,passes=1;

    if (map_fp != NULL)
        passes = 2;

    for (pass=0;pass < passes;pass++) {
        i = 0;

        if (map_fp != NULL) {
            fprintf(map_fp,"\n");
            fprintf(map_fp,"Symbol table %s: %u entries\n",pass == 0 ? "by name" : "by address",(unsigned int)link_symbols.size());
            fprintf(map_fp,"---------------------------------------\n");
        }

        if (cmdoptions.verbose || map_fp != NULL)
            sort(link_symbols.begin(), link_symbols.end(), pass == 0 ? link_symbol_qsort_cmp_by_name : link_symbol_qsort_cmp);

        while (i < link_symbols.size()) {
            shared_ptr<struct link_symbol> sym = link_symbols[i++];

            if (cmdoptions.verbose) {
                fprintf(stderr,"symbol[%u]: name='%s' group='%s' seg='%s' offset=0x%lx frag=%p file='%s' module=%u local=%u\n",
                        i/*post-increment, intentional*/,sym->name.c_str(),sym->groupdef.c_str(),sym->segref->name.c_str(),(unsigned long)sym->offset,(void*)sym->fragment.get(),
                        get_in_file(sym->in_file),sym->in_module,sym->is_local);
            }

            if (map_fp != NULL) {
                shared_ptr<struct link_segdef> sg = sym->segref;
                shared_ptr<struct seg_fragment> frag = sym->fragment;

                fprintf(map_fp,"  %-32s %c %04lx:%08lx [0x%08lx] %20s + 0x%08lx from '%s'",
                        sym->name.c_str(),
                        sym->is_local?'L':'G',
                        (unsigned long)sg->segment_relative&0xfffful,
                        (unsigned long)sg->segment_offset + (unsigned long)frag->offset + (unsigned long)sym->offset,
                        (unsigned long)sg->linear_offset + (unsigned long)frag->offset + (unsigned long)sym->offset,
                        sym->segref->name.c_str(),
                        (unsigned long)frag->offset + (unsigned long)sym->offset,
                        get_in_file(sym->in_file));

                if (sym->in_module != in_fileModuleRefUndef) {
                    fprintf(map_fp,":%u",
                            sym->in_module);
                }
                if (sg->segment_group >= 0) {
                    fprintf(map_fp," group=%d",
                            sg->segment_group);
                }

                fprintf(map_fp,"\n");
            }
        }

        if (map_fp != NULL)
            fprintf(map_fp,"\n");
    }
}

enum {
    DUMPLS_LINEAR,
    DUMPLS_FILEOFFSET
};

void dump_link_segments(const unsigned int purpose) {
    static char range1[64];
    static char range2[64];
    unsigned int i=0,f;
    const char *what;

    switch (purpose) {
        case DUMPLS_LINEAR:
            what = "linear address";
            break;
        case DUMPLS_FILEOFFSET:
            what = "file offset";
            break;
        default:
            abort();
            break;
    }

    if (map_fp != NULL) {
        fprintf(map_fp,"\n");
        fprintf(map_fp,"Segment table: %u entries (memory, %s)\n",(unsigned int)link_segments.size(),what);
        fprintf(map_fp,"-----------------------------------------------------\n");
    }

    while (i < link_segments.size()) {
        shared_ptr<struct link_segdef> sg = link_segments[i++];

        if (map_fp != NULL) {
            if (sg->segment_length != 0ul && sg->segment_offset != segmentOffsetUndef) {
                sprintf(range1,"%08lx-%08lx",
                    (unsigned long)sg->segment_offset,
                    (unsigned long)sg->segment_offset+(unsigned long)sg->segment_length-1ul);
            }
            else {
                strcpy(range1,"-----------------");
            }

            if (purpose == DUMPLS_FILEOFFSET) {
                if (sg->segment_length != 0ul && sg->file_offset != fileOffsetUndef) {
                    sprintf(range2,"0x%08lx-0x%08lx",
                        (unsigned long)sg->file_offset,
                        (unsigned long)sg->file_offset+sg->segment_length-1ul);
                }
                else {
                    strcpy(range2,"---------------------");
                }
            }
            else {
                if (sg->segment_length != 0ul && sg->linear_offset != linearAddressUndef) {
                    sprintf(range2,"0x%08lx-0x%08lx",
                        (unsigned long)sg->linear_offset,
                        (unsigned long)sg->linear_offset+sg->segment_length-1ul);
                }
                else {
                    strcpy(range2,"---------------------");
                }
            }

            fprintf(map_fp,"  [use%02u] %-20s %-20s %-20s %04lx:%s [%s] base=0x%04lx align=%lu%s%s%s",
                sg->attr.f.f.use32?32:16,
                sg->name.c_str(),
                sg->classname.c_str(),
                sg->groupname.c_str(),
                (unsigned long)sg->segment_relative&0xfffful,
                range1,
                range2,
                (unsigned long)sg->segment_base,
                (unsigned long)alignMaskToValue(sg->segment_alignment),
                sg->pinned ? " PIN" : "",
                sg->noemit ? " NOEMIT" : "",
                sg->header ? " HEADER" : "");

            if (sg->segment_group >= 0) {
                fprintf(map_fp," group=%d",
                        sg->segment_group);
            }

            fprintf(map_fp,"\n");
        }

        for (f=0;f < sg->fragments.size();f++) {
            shared_ptr<struct seg_fragment> frag = sg->fragments[f];

            if (map_fp != NULL) {
                if (frag->fragment_length != 0ul && sg->segment_offset != segmentOffsetUndef) {
                    sprintf(range1,"%08lx-%08lx",
                        (unsigned long)sg->segment_offset+(unsigned long)frag->offset,
                        (unsigned long)sg->segment_offset+(unsigned long)frag->offset+(unsigned long)frag->fragment_length-1ul);
                }
                else {
                    strcpy(range1,"-----------------");
                }

                if (purpose == DUMPLS_FILEOFFSET) {
                    if (frag->fragment_length != 0ul && sg->file_offset != fileOffsetUndef) {
                        sprintf(range2,"0x%08lx-0x%08lx",
                            (unsigned long)sg->file_offset+(unsigned long)frag->offset,
                            (unsigned long)sg->file_offset+(unsigned long)frag->offset+(unsigned long)frag->fragment_length-1ul);
                    }
                    else {
                        strcpy(range2,"---------------------");
                    }
                }
                else {
                    if (frag->fragment_length != 0ul && sg->linear_offset != linearAddressUndef) {
                        sprintf(range2,"0x%08lx-0x%08lx",
                            (unsigned long)sg->linear_offset+(unsigned long)frag->offset,
                            (unsigned long)sg->linear_offset+(unsigned long)frag->offset+(unsigned long)frag->fragment_length-1ul);
                    }
                    else {
                        strcpy(range2,"---------------------");
                    }
                }

                fprintf(map_fp,"  [use%02u] %-20s %-20s %-20s      %s [%s]   from '%s'",
                        frag->attr.f.f.use32?32:16,
                        "",
                        "",
                        "",
                        range1,
                        range2,
                        get_in_file(frag->in_file));

                if (frag->in_module != in_fileModuleRefUndef) {
                    fprintf(map_fp,":%u",
                            frag->in_module);
                }

                fprintf(map_fp," align=%lu\n",
                        (unsigned long)alignMaskToValue(frag->fragment_alignment));
            }
        }

        if (i < link_segments.size() && sg->segment_length != 0ul) {
            shared_ptr<struct link_segdef> nsg = link_segments[i];

            if (map_fp != NULL) {
                range1[0] = 0;
                range2[0] = 0;
                if (purpose == DUMPLS_FILEOFFSET) {
                    if (sg->file_offset != fileOffsetUndef && nsg->file_offset != fileOffsetUndef && (sg->file_offset+(fileOffset)sg->segment_length) < nsg->file_offset) {
                        const fileOffset gap = nsg->file_offset - (sg->file_offset+(fileOffset)sg->segment_length);

                        sprintf(range1,"%08lx-%08lx",
                                (unsigned long)sg->segment_offset+(unsigned long)sg->segment_length,
                                (unsigned long)sg->segment_offset+(unsigned long)sg->segment_length+(unsigned long)gap-1ul);

                        sprintf(range2,"0x%08lx-0x%08lx",
                                (unsigned long)sg->file_offset+(unsigned long)sg->segment_length,
                                (unsigned long)nsg->file_offset-1ul);
                    }
                    else {
                        strcpy(range1,"-----------------");
                        strcpy(range2,"---------------------");
                    }
                }
                else {
                    if (sg->linear_offset != linearAddressUndef && nsg->linear_offset != linearAddressUndef && (sg->linear_offset+(linearAddress)sg->segment_length) < nsg->linear_offset) {
                        const fileOffset gap = nsg->linear_offset - (sg->linear_offset+(linearAddress)sg->segment_length);

                        sprintf(range1,"%08lx-%08lx",
                                (unsigned long)sg->segment_offset+(unsigned long)sg->segment_length,
                                (unsigned long)sg->segment_offset+(unsigned long)sg->segment_length+(unsigned long)gap-1ul);

                        sprintf(range2,"0x%08lx-0x%08lx",
                                (unsigned long)sg->linear_offset+(unsigned long)sg->segment_length,
                                (unsigned long)nsg->linear_offset-1ul);
                    }
                    else {
                        strcpy(range1,"-----------------");
                        strcpy(range2,"---------------------");
                    }
                }

                if (range1[0] != 0 && range1[0] != '-') {
                    fprintf(map_fp,"  [ pad ]                                                                     %s [%s]   padding\n",
                            range1,
                            range2);
                }
            }
        }
    }

    if (map_fp != NULL)
        fprintf(map_fp,"\n");
}

shared_ptr<struct link_segdef> find_link_segment_by_grpdef(const char *name) {
    size_t i=0;

    while (i < link_segments.size()) {
        shared_ptr<struct link_segdef> sg = link_segments[i++];
        if (sg->groupname == name) return sg;
    }

    return nullptr;
}

shared_ptr<struct link_segdef> find_link_segment_by_class(const char *name) {
    size_t i=0;

    while (i < link_segments.size()) {
        shared_ptr<struct link_segdef> sg = link_segments[i++];
        if (sg->classname == name) return sg;
    }

    return nullptr;
}

shared_ptr<struct link_segdef> find_link_segment_by_class_last(const char *name) {
    shared_ptr<struct link_segdef> ret;
    size_t i=0;

    while (i < link_segments.size()) {
        shared_ptr<struct link_segdef> sg = link_segments[i++];
        if (sg->classname == name) ret = sg;
    }

    return ret;
}

fragmentRef find_link_segment_by_file_module_and_segment_index(const struct link_segdef * const sg,const in_fileRef in_file,const in_fileModuleRef in_module,const segmentIndex TargetDatum) {
    for (auto i=sg->fragments.begin();i != sg->fragments.end();i++) {
        const struct seg_fragment *f = (*i).get();

        if (f->in_file == in_file && f->in_module == in_module && f->from_segment_index == TargetDatum)
            return (*i);
    }

    return fragmentRefUndef;
}

/* Try to return the segment matching current segment group or higher.
 * Presumably segdefs are sorted such that segment groups increment sequentially
 * given a specific name. */
shared_ptr<struct link_segdef> find_link_segment(const char *name) {
    shared_ptr<struct link_segdef> sg,rsg;
    size_t i=0;

    while (i < link_segments.size()) {
        sg = link_segments[i++];
        if (sg->name == name) {
            rsg = sg;

            if (sg->segment_group >= current_segment_group)
                break;
        }
    }

    return rsg;
}

shared_ptr<struct link_segdef> new_link_segment(const char *name) {
    shared_ptr<struct link_segdef> sg(new struct link_segdef);
    link_segments.push_back(sg);
    sg->name = name;
    return sg;
}

shared_ptr<struct link_segdef> new_link_segment_begin(const char *name) {
    shared_ptr<struct link_segdef> sg(new struct link_segdef);
    link_segments.insert(link_segments.begin(), sg );
    sg->name = name;
    return sg;
}

int ledata_add(struct omf_context_t *omf_state, struct omf_ledata_info_t *info,in_fileRef in_file,in_fileModuleRef in_module,unsigned int pass) {
    (void)in_module;
    (void)in_file;

    const char *segname = omf_context_get_segdef_name_safe(omf_state, info->segment_index);
    if (*segname == 0) {
        fprintf(stderr,"Null segment name\n");
        return 1;
    }

    shared_ptr<struct link_segdef> lsg = find_link_segment(segname);

    if (lsg == NULL) {
        fprintf(stderr,"Segment %s not found\n",segname);
        return 1;
    }

    if (lsg->noemit) return 0;

    shared_ptr<struct link_segdef> current_link_segment = lsg;

    if (info->data_length == 0)
        return 0;

    if (lsg->fragments.empty()) {
        fprintf(stderr,"LEDATA when no fragments defined (bug?)\n");
        return 1;
    }

    /* NTS: For simplicity reasons, load_index is cleared before every object file,
     *      and is set by SEGDEF parsing which allows only ONE fragment from a segment
     *      in an individual object file. */
    shared_ptr<struct seg_fragment> frag = lsg->fragment_load_index;

    unsigned long max_ofs = (unsigned long)info->enum_data_offset + (unsigned long)info->data_length;
    if (max_ofs > frag->fragment_length) {
        fprintf(stderr,"LEDATA out of fragment bounds (len=%lu max=%lu)\n",(unsigned long)frag->fragment_length,max_ofs);
        return 1;
    }

    if (pass == PASS_BUILD) {
        assert(info->data != NULL);
        assert(frag->image.size() == frag->fragment_length);
        assert(max_ofs >= (unsigned long)info->data_length);
        max_ofs -= (unsigned long)info->data_length;
        memcpy(&frag->image[max_ofs], info->data, info->data_length);
    }

    return 0;
}

int fixupp_get(struct omf_context_t *omf_state,unsigned long *fseg,unsigned long *fofs,shared_ptr<struct link_segdef> *sdef,const struct omf_fixupp_t *ent,unsigned int method,unsigned int index,in_fileRef in_file,in_fileModuleRef in_module) {
    *fseg = *fofs = ~0UL;
    *sdef = NULL;
    (void)ent;

    if (method == 0/*SEGDEF*/) {
        const char *segname = omf_context_get_segdef_name_safe(omf_state,index);
        if (*segname == 0) {
            fprintf(stderr,"FIXUPP SEGDEF no name\n");
            return -1;
        }

        shared_ptr<struct link_segdef> lsg = find_link_segment(segname);
        if (lsg == NULL) {
            fprintf(stderr,"FIXUPP SEGDEF not found '%s'\n",segname);
            return -1;
        }

        auto frag = lsg->fragment_load_index;
        assert(frag != nullptr);

        *fseg = lsg->segment_relative;
        *fofs = lsg->segment_offset + frag->offset;
        *sdef = lsg;
    }
    else if (method == 1/*GRPDEF*/) {
        const char *segname = omf_context_get_grpdef_name_safe(omf_state,index);
        if (*segname == 0) {
            fprintf(stderr,"FIXUPP SEGDEF no name\n");
            return -1;
        }

        shared_ptr<struct link_segdef> lsg = find_link_segment_by_grpdef(segname);
        if (lsg == NULL) {
            fprintf(stderr,"FIXUPP SEGDEF not found\n");
            return -1;
        }

        auto frag = lsg->fragment_load_index;
        assert(frag != nullptr);

        *fseg = lsg->segment_relative;
        *fofs = lsg->segment_offset + frag->offset;
        *sdef = lsg;
    }
    else if (method == 2/*EXTDEF*/) {
        const char *defname = omf_context_get_extdef_name_safe(omf_state,index);
        if (*defname == 0) {
            fprintf(stderr,"FIXUPP EXTDEF no name\n");
            return -1;
        }

        shared_ptr<struct link_symbol> sym = find_link_symbol(defname,in_file,in_module);
        if (sym == NULL) {
            fprintf(stderr,"No such symbol '%s'\n",defname);
            return -1;
        }

        shared_ptr<struct seg_fragment> frag = sym->fragment;
        shared_ptr<struct link_segdef> lsg = sym->segref;

        *fseg = lsg->segment_relative;
        *fofs = sym->offset + lsg->segment_offset + frag->offset;
        *sdef = lsg;
    }
    else if (method == 5/*BY TARGET*/) {
    }
    else {
        fprintf(stderr,"FRAME UNSUPP not impl\n");
    }

    return 0;
}

int apply_FIXUPP(struct omf_context_t *omf_state,unsigned int first,in_fileRef in_file,in_fileModuleRef in_module,unsigned int pass) {
    shared_ptr<struct link_segdef> frame_sdef;
    shared_ptr<struct link_segdef> targ_sdef;
    const struct omf_segdef_t *cur_segdef;
    shared_ptr<struct seg_fragment> frag;
    unsigned long final_seg,final_ofs;
    unsigned long frame_seg,frame_ofs;
    unsigned long targ_seg,targ_ofs;
    const char *cur_segdefname;
    unsigned char *fence;
    unsigned char *ptr;
    unsigned long ptch;

    while (first <= omf_fixupps_context_get_highest_index(&omf_state->FIXUPPs)) {
        const struct omf_fixupp_t *ent = omf_fixupps_context_get_fixupp(&omf_state->FIXUPPs,first++);
        if (ent == NULL) continue;
        if (!ent->alloc) continue;

        if (pass == PASS_BUILD) {
            if (fixupp_get(omf_state,&frame_seg,&frame_ofs,&frame_sdef,ent,ent->frame_method,ent->frame_index,in_file,in_module))
                return -1;
            if (fixupp_get(omf_state,&targ_seg,&targ_ofs,&targ_sdef,ent,ent->target_method,ent->target_index,in_file,in_module))
                return -1;

            if (ent->frame_method == 5/*BY TARGET*/) {
                frame_sdef = targ_sdef;
                frame_seg = targ_seg;
                frame_ofs = targ_ofs;
            }

            if (omf_state->flags.verbose) {
                fprintf(stderr,"fixup[%u] frame=%lx:%lx targ=%lx:%lx\n",
                        first,
                        frame_seg,frame_ofs,
                        targ_seg,targ_ofs);
            }

            if (frame_seg == ~0UL || frame_ofs == ~0UL || frame_sdef == NULL) {
                fprintf(stderr,"frame addr not resolved\n");
                continue;
            }
            if (targ_seg == ~0UL || targ_ofs == ~0UL || targ_sdef == NULL) {
                fprintf(stderr,"target addr not resolved\n");
                continue;
            }

            final_seg = targ_seg;
            final_ofs = targ_ofs;

            if (final_seg != frame_seg) {
                fprintf(stderr,"frame!=target seg not supported\n");
                continue;
            }

            if (omf_state->flags.verbose) {
                fprintf(stderr,"fixup[%u] final=%lx:%lx\n",
                        first,
                        final_seg,final_ofs);
            }
        }
        else {
            final_seg = 0;
            final_ofs = 0;
            frame_seg = 0;
            frame_ofs = 0;
            targ_seg = 0;
            targ_ofs = 0;
            frame_sdef = NULL;
            targ_sdef = NULL;
        }

        cur_segdef = omf_segdefs_context_get_segdef(&omf_state->SEGDEFs,ent->fixup_segdef_index);
        if (cur_segdef == NULL) {
            fprintf(stderr,"Cannot find OMF SEGDEF\n");
            return 1;
        }
        cur_segdefname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,cur_segdef->segment_name_index);
        if (*cur_segdefname == 0) {
            fprintf(stderr,"Cannot resolve OMF SEGDEF name\n");
            return 1;
        }

        shared_ptr<struct link_segdef> current_link_segment = find_link_segment(cur_segdefname);
        if (current_link_segment == NULL) {
            fprintf(stderr,"Cannot find linker segment '%s'\n",cur_segdefname);
            return 1;
        }

        /* assuming each OBJ/module has only one of each named segment,
         * get the fragment it belongs to */
        assert(current_link_segment != nullptr);
        assert(current_link_segment->fragment_load_index != nullptr);
        frag = current_link_segment->fragment_load_index;

        assert(frag->in_file == in_file);
        assert(frag->in_module == in_module);

        if (pass == PASS_BUILD) {
            assert(frag->image.size() == frag->fragment_length);
            fence = &frag->image[frag->fragment_length];

            ptch = (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset + (unsigned long)frag->offset;

            assert(ptch >= frag->offset);
            assert((ptch-frag->offset) < frag->image.size());

            ptr = &frag->image[ptch-frag->offset];
            assert(ptr < fence);
        }
        else if (pass == PASS_GATHER) {
            ptr = fence = NULL;
            ptch = 0;
        }
        else {
            continue;
        }

        switch (ent->location) {
            case OMF_FIXUPP_LOCATION_16BIT_OFFSET: /* 16-bit offset */
                if (pass == PASS_BUILD) {
                    assert((ptr+2) <= fence);

                    if (!ent->segment_relative) {
                        /* sanity check: self-relative is only allowed IF the same segment */
                        /* we could fidget about with relative fixups across real-mode segments, but I'm not going to waste my time on that */
                        if (current_link_segment->segment_relative != targ_sdef->segment_relative) {
                            dump_link_segments(DUMPLS_LINEAR);
                            fprintf(stderr,"FIXUPP: self-relative offset fixup across segments with different bases not allowed\n");
                            fprintf(stderr,"        FIXUP in segment '%s' base 0x%lx\n",
                                current_link_segment->name.c_str(),
                                (unsigned long)current_link_segment->segment_relative);
                            fprintf(stderr,"        FIXUP to segment '%s' base 0x%lx\n",
                                targ_sdef->name.c_str(),
                                (unsigned long)targ_sdef->segment_relative);
                            return -1;
                        }

                        /* do it */
                        final_ofs -= ptch+2+current_link_segment->segment_offset;
                    }

                    *((uint16_t*)ptr) += (uint16_t)final_ofs;
                }
                break;
            case OMF_FIXUPP_LOCATION_16BIT_SEGMENT_BASE: /* 16-bit segment base */
                if (pass == PASS_BUILD) {
                    assert((ptr+2) <= fence);

                    if (!ent->segment_relative) {
                        fprintf(stderr,"segment base self relative\n");
                        return -1;
                    }
                }

                if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_DOSDRV) {
                    if (!(cmdoptions.output_format_variant == OFMTVAR_COMREL)) {
                        fprintf(stderr,"segment base self-relative not supported for .COM\n");
                        return -1;
                    }
                }

                if (pass == PASS_GATHER) {
                    shared_ptr<struct exe_relocation> reloc = new_exe_relocation();
                    if (reloc == NULL) {
                        fprintf(stderr,"Unable to allocate relocation\n");
                        return -1;
                    }

                    assert(current_link_segment->fragment_load_index != fragmentRefUndef);
                    reloc->segref = current_link_segment;
                    reloc->fragment = current_link_segment->fragment_load_index;
                    reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset;

                    if (cmdoptions.verbose)
                        fprintf(stderr,"Relocation entry: Patch up %s:%p:%04lx\n",reloc->segref->name.c_str(),(void*)reloc->fragment.get(),(unsigned long)reloc->offset);
                }

                if (pass == PASS_BUILD) {
                    *((uint16_t*)ptr) += (uint16_t)targ_sdef->segment_relative + (uint16_t)targ_sdef->segment_reloc_adj;
                }

                break;
            case OMF_FIXUPP_LOCATION_16BIT_SEGMENT_OFFSET: /* 16-bit segment:offset */
                if (pass == PASS_BUILD) {
                    assert((ptr+4) <= fence);

                    if (!ent->segment_relative) {
                        fprintf(stderr,"segment base self relative\n");
                        return -1;
                    }
                }

                if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_DOSDRV) {
                    if (!(cmdoptions.output_format_variant == OFMTVAR_COMREL)) {
                        fprintf(stderr,"segment base self-relative not supported for .COM\n");
                        return -1;
                    }
                }

                if (pass == PASS_GATHER) {
                    shared_ptr<struct exe_relocation> reloc = new_exe_relocation();
                    if (reloc == NULL) {
                        fprintf(stderr,"Unable to allocate relocation\n");
                        return -1;
                    }

                    assert(current_link_segment->fragment_load_index != fragmentRefUndef);
                    reloc->segref = current_link_segment;
                    reloc->fragment = current_link_segment->fragment_load_index;
                    reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset + 2u;

                    if (cmdoptions.verbose)
                        fprintf(stderr,"Relocation entry: Patch up %s:%p:%04lx\n",reloc->segref->name.c_str(),(void*)reloc->fragment.get(),(unsigned long)reloc->offset);
                }

                if (pass == PASS_BUILD) {
                    *((uint16_t*)ptr) += (uint16_t)final_ofs;
                    *((uint16_t*)(ptr+2)) += (uint16_t)targ_sdef->segment_relative + (uint16_t)targ_sdef->segment_reloc_adj;
                }

                break;
            case OMF_FIXUPP_LOCATION_32BIT_OFFSET: /* 32-bit offset */
                if (pass == PASS_BUILD) {
                    assert((ptr+4) <= fence);

                    if (!ent->segment_relative) {
                        /* sanity check: self-relative is only allowed IF the same segment */
                        /* we could fidget about with relative fixups across real-mode segments, but I'm not going to waste my time on that */
                        if (current_link_segment->segment_relative != targ_sdef->segment_relative) {
                            dump_link_segments(DUMPLS_LINEAR);
                            fprintf(stderr,"FIXUPP: self-relative offset fixup across segments with different bases not allowed\n");
                            fprintf(stderr,"        FIXUP in segment '%s' base 0x%lx\n",
                                current_link_segment->name.c_str(),
                                (unsigned long)current_link_segment->segment_relative);
                            fprintf(stderr,"        FIXUP to segment '%s' base 0x%lx\n",
                                targ_sdef->name.c_str(),
                                (unsigned long)targ_sdef->segment_relative);
                            return -1;
                        }

                        /* do it */
                        final_ofs -= ptch+4+current_link_segment->segment_offset;
                    }

                    *((uint32_t*)ptr) += (uint16_t)final_ofs;
                }
                break;
 
            default:
                fprintf(stderr,"Unsupported fixup\n");
                return -1;
        }
    }

    return 0;
}

int grpdef_add(struct omf_context_t *omf_state,unsigned int first) {
    while (first < omf_state->GRPDEFs.omf_GRPDEFS_count) {
        struct omf_grpdef_t *gd = &omf_state->GRPDEFs.omf_GRPDEFS[first++];
        const char *grpdef_name;
        const char *segdef_name;
        unsigned int i;
        int segdef;

        grpdef_name = omf_lnames_context_get_name_safe(&omf_state->LNAMEs, gd->group_name_index);
        if (*grpdef_name == 0) continue;

        for (i=0;i < gd->count;i++) {
            segdef = omf_grpdefs_context_get_grpdef_segdef(&omf_state->GRPDEFs,gd,i);
            if (segdef >= 0) {
                const struct omf_segdef_t *sg = omf_segdefs_context_get_segdef(&omf_state->SEGDEFs,segdef);

                if (sg == NULL) {
                    fprintf(stderr,"GRPDEF refers to non-existent SEGDEF\n");
                    return 1;
                }

                segdef_name = omf_lnames_context_get_name_safe(&omf_state->LNAMEs, sg->segment_name_index);
                if (*segdef_name == 0) {
                    fprintf(stderr,"GRPDEF refers to SEGDEF with no name\n");
                    return 1;
                }

                shared_ptr<struct link_segdef> lsg = find_link_segment(segdef_name);
                if (lsg == NULL) {
                    fprintf(stderr,"GRPDEF refers to SEGDEF that has not been registered\n");
                    return 1;
                }

                if (lsg->groupname.empty()) {
                    /* assign to group */
                    lsg->groupname = grpdef_name;
                }
                else if (lsg->groupname == grpdef_name) {
                    /* re-asserting group membership, OK */
                }
                else {
                    fprintf(stderr,"GRPDEF re-defines membership of segment '%s'\n",segdef_name);
                    return 1;
                }
            }
        }
    }

    return 0;
}

int pubdef_add(struct omf_context_t *omf_state,unsigned int first,unsigned int tag,in_fileRef in_file,in_fileModuleRef in_module,unsigned int pass) {
    const unsigned char is_local = (tag == OMF_RECTYPE_LPUBDEF) || (tag == OMF_RECTYPE_LPUBDEF32);

    (void)pass;

    while (first < omf_state->PUBDEFs.omf_PUBDEFS_count) {
        shared_ptr<struct link_symbol> sym;
        const struct omf_pubdef_t *pubdef = &omf_state->PUBDEFs.omf_PUBDEFS[first++];
        const char *groupname;
        const char *segname;
        const char *name;

        if (pubdef == NULL) continue;
        name = pubdef->name_string;
        if (name == NULL) continue;
        segname = omf_context_get_segdef_name_safe(omf_state,pubdef->segment_index);
        if (*segname == 0) continue;
        groupname = omf_context_get_grpdef_name_safe(omf_state,pubdef->group_index);

        shared_ptr<struct link_segdef> lsg = find_link_segment(segname);
        if (lsg == NULL) {
            fprintf(stderr,"Pubdef: No such segment '%s'\n",segname);
            return -1;
        }

        if (cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRVEXE) {
        }
        else {
            /* no symbols allowed in STACK.
             * BSS is allowed. */
            if (!strcasecmp(segname,"_STACK") || !strcasecmp(segname,"STACK") || lsg->classname == "STACK") {
                fprintf(stderr,"Emitting symbols to STACK segment not permitted for COM/DRV output\n");
                return 1;
            }
        }

        sym = find_link_symbol(name,in_file,in_module);
        if (sym != NULL) {
            fprintf(stderr,"Symbol '%s' already defined\n",name);
            return -1;
        }
        sym = new_link_symbol(name);
        if (sym == NULL) {
            fprintf(stderr,"Unable to allocate symbol '%s'\n",name);
            return -1;
        }

        assert(pass == PASS_GATHER);
        assert(!lsg->fragments.empty());
        assert(lsg->fragment_load_index != nullptr);

        /* NTS: For simplicity reasons, load_index is cleared before every object file,
         *      and is set by SEGDEF parsing which allows only ONE fragment from a segment
         *      in an individual object file. */

        sym->fragment = lsg->fragment_load_index;
        sym->offset = pubdef->public_offset;
        sym->groupdef = groupname;
        sym->segref = lsg;
        sym->in_file = in_file;
        sym->in_module = in_module;
        sym->is_local = is_local;
    }

    return 0;
}

int segdef_add(struct omf_context_t *omf_state,unsigned int first,in_fileRef in_file,in_fileModuleRef in_module,unsigned int pass) {
    while (first < omf_state->SEGDEFs.omf_SEGDEFS_count) {
        struct omf_segdef_t *sg = &omf_state->SEGDEFs.omf_SEGDEFS[first++];
        const char *classname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->class_name_index);
        const char *name = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->segment_name_index);

        if (*name == 0) continue;

        if (pass == PASS_BUILD) {
            shared_ptr<struct link_segdef> lsg = find_link_segment(name);
            if (lsg == NULL) {
                fprintf(stderr,"No SEGDEF '%s'\n",name);
                return -1;
            }
            if (lsg->fragments.empty()) {
                fprintf(stderr,"SEGDEF '%s' with no fragments on second pass?\n",name);
                return -1;
            }

            /* match fragment to input file, input module.
             * Remember: For simplicity reasons, only ONE FRAGMENT per segment per module. We do not allow an OBJ to SEGDEF the same name twice. */
            lsg->fragment_load_index = nullptr;
            for (auto fi=lsg->fragments.begin();fi!=lsg->fragments.end();fi++) {
                if ((*fi)->in_file == in_file && (*fi)->in_module == in_module && (*fi)->from_segment_index == first) {
                    lsg->fragment_load_index = *fi;
                    break;
                }
            }

            if (lsg->fragment_load_index == nullptr) {
                fprintf(stderr,"SEGDEF '%s', second pass, failed to find fragment\n",name);
                dump_link_segments(DUMPLS_LINEAR);
                return -1;
            }

            {
                shared_ptr<struct seg_fragment> f = lsg->fragment_load_index;

                assert(f->in_file == in_file);
                assert(f->in_module == in_module);
                assert(f->from_segment_index == first); /* NTS: remember the code above does first++, so this is 1 based, like OMF */
                assert(f->fragment_length == sg->segment_length);
            }
        }
        else if (pass == PASS_GATHER) {
            shared_ptr<struct link_segdef> lsg = find_link_segment(name);

            if (lsg != nullptr) {
                if (lsg->segment_group < current_segment_group)
                    lsg = nullptr;
            }

            if (lsg != NULL) {
                /* it is an error to change attributes */
                if (cmdoptions.verbose)
                    fprintf(stderr,"SEGDEF class='%s' name='%s' already exits\n",classname,name);

                if (lsg->attr.f.f.combination != sg->attr.f.f.combination ||
                    lsg->attr.f.f.big_segment != sg->attr.f.f.big_segment) {
                    fprintf(stderr,"ERROR, segment attribute changed\n");
                    return -1;
                }
            }
            else {
                if (cmdoptions.verbose)
                    fprintf(stderr,"Adding class='%s' name='%s'\n",classname,name);

                lsg = new_link_segment(name);
                if (lsg == NULL) {
                    fprintf(stderr,"Cannot add segment\n");
                    return -1;
                }

                lsg->attr = sg->attr;
                lsg->classname = classname;
                lsg->segment_group = current_segment_group;
            }

            /* We no longer allow one OBJ file/module to define the same segment twice.
             * That way, only one fragment exists for a segment, one from each object file. */
            if (!lsg->fragments.empty()) {
                shared_ptr<struct seg_fragment> f = lsg->fragments.back(); /* last entry */

                if (f->in_file == in_file && f->in_module == in_module) {
                    fprintf(stderr,"Segment '%s' defined more than once in an individual object file\n",name);
                    return -1;
                }
            }

            {
                shared_ptr<struct seg_fragment> f = alloc_link_segment_fragment(lsg.get());
                lsg->fragment_load_index = f;

                f->in_file = in_file;
                f->in_module = in_module;
                f->from_segment_index = first; /* NTS: remember the code above does first++, so this is 1 based, like OMF */
                f->fragment_length = sg->segment_length;
                f->fragment_alignment = alignValueToAlignMask(omf_align_code_to_bytes(sg->attr.f.f.alignment));
                f->attr = sg->attr;

                /* The larger of the two alignments takes effect. This is possible because alignment is a bitmask */
                lsg->segment_alignment &= alignValueToAlignMask(omf_align_code_to_bytes(sg->attr.f.f.alignment));
            }
        }
    }

    return 0;
}

static void help(void) {
    fprintf(stderr,"lnkdos16 [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to link\n");
    fprintf(stderr,"  -o <file>    Output file\n");
    fprintf(stderr,"  -map <file>  Map/report file\n");
    fprintf(stderr,"  -of <fmt>    Output format (COM, EXE, COMREL)\n");
    fprintf(stderr,"                COM = flat COM executable\n");
    fprintf(stderr,"                COMREL = flat COM executable, relocatable\n");
    fprintf(stderr,"                EXE = segmented EXE executable\n");
    fprintf(stderr,"                DOSDRV = flat MS-DOS driver (SYS)\n");
    fprintf(stderr,"                DOSDRVREL = flat MS-DOS driver (SYS), relocateable\n");
    fprintf(stderr,"                DOSDRVEXE = MS-DOS driver (EXE)\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
    fprintf(stderr,"  -no-dosseg   No DOSSEG sort order\n");
    fprintf(stderr,"  -dosseg      DOSSEG sort order\n");
    fprintf(stderr,"  -seggroup    Start new segment group, for OBJs after, i.e. init section\n");
    fprintf(stderr,"  -comNNN      Link .COM segment starting at 0xNNN\n");
    fprintf(stderr,"  -com100      Link .COM segment starting at 0x100\n");
    fprintf(stderr,"  -com0        Link .COM segment starting at 0 (Watcom Linker)\n");
    fprintf(stderr,"  -stackN      Set minimum stack segment size (0xNNN)\n");
    fprintf(stderr,"  -pflat       Prefer .COM-like flat layout\n");
    fprintf(stderr,"  -hsym        Header symbol name (DOSDRV)\n");
    fprintf(stderr,"  -hex <file>  Also emit file as C header hex dump\n");
    fprintf(stderr,"  -hexsplit    Emit to -hex as .h and .c files\n");
    fprintf(stderr,"  -hexcpp      Use CPP extension.\n");
}

void my_dumpstate(const struct omf_context_t * const ctx) {
    unsigned int i;
    const char *p;

    printf("OBJ dump state:\n");

    if (ctx->THEADR != NULL)
        printf("* THEADR: \"%s\"\n",ctx->THEADR);

    if (ctx->LNAMEs.omf_LNAMES != NULL) {
        printf("* LNAMEs:\n");
        for (i=1;i <= ctx->LNAMEs.omf_LNAMES_count;i++) {
            p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

            if (p != NULL)
                printf("   [%u]: \"%s\"\n",i,p);
            else
                printf("   [%u]: (null)\n",i);
        }
    }

    if (ctx->SEGDEFs.omf_SEGDEFS != NULL) {
        for (i=1;i <= ctx->SEGDEFs.omf_SEGDEFS_count;i++)
            dump_SEGDEF(stdout,omf_state,i);
    }

    if (ctx->GRPDEFs.omf_GRPDEFS != NULL) {
        for (i=1;i <= ctx->GRPDEFs.omf_GRPDEFS_count;i++)
            dump_GRPDEF(stdout,omf_state,i);
    }

    if (ctx->EXTDEFs.omf_EXTDEFS != NULL)
        dump_EXTDEF(stdout,omf_state,1);

    if (ctx->PUBDEFs.omf_PUBDEFS != NULL)
        dump_PUBDEF(stdout,omf_state,1);

    if (ctx->FIXUPPs.omf_FIXUPPS != NULL)
        dump_FIXUPP(stdout,omf_state,1);

    if (cmdoptions.verbose)
        printf("----END-----\n");
}

int trim_noemit() {
    /* decide where the segments end up in the executable */
    unsigned int linkseg;

    for (linkseg=link_segments.size();linkseg > 0;) {
        shared_ptr<struct link_segdef> sd = link_segments[--linkseg];
        if (!sd->noemit) break;
    }

    for (;linkseg > 0;) {
        shared_ptr<struct link_segdef> sd = link_segments[--linkseg];

        if (sd->noemit) {
            fprintf(stderr,"Warning, segment '%s' marked NOEMIT will be emitted due to COM/EXE format constraints.\n",sd->name.c_str());

            if (map_fp != NULL)
                fprintf(map_fp,"* Warning, segment '%s' marked NOEMIT will be emitted due to COM/EXE format constraints.\n",sd->name.c_str());

            sd->noemit = 0;
        }
    }

    return 0;
}

int segment_exe_arrange(void) {
    if (cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRVEXE) {
        unsigned long segrel = 0;
        unsigned int linkseg;

        for (linkseg=0;linkseg < link_segments.size();linkseg++) {
            shared_ptr<struct link_segdef> sd = link_segments[linkseg];
            shared_ptr<struct link_segdef> gd = find_link_segment_by_grpdef(sd->groupname.c_str());
            shared_ptr<struct link_segdef> cd = find_link_segment_by_class(sd->classname.c_str());

            if (gd != NULL)
                segrel = gd->linear_offset >> 4ul;
            else if (cd != NULL)
                segrel = cd->linear_offset >> 4ul;
            else
                segrel = sd->linear_offset >> 4ul;

            if (cmdoptions.prefer_flat && sd->linear_offset < (0xFFFFul - cmdoptions.image_base_offset))
                segrel = 0; /* user prefers flat .COM memory model, where possible */

            sd->segment_base = cmdoptions.image_base_offset;
            sd->segment_relative = segrel + cmdoptions.image_base_segment;
            sd->segment_reloc_adj = cmdoptions.image_base_segment_reloc_adjust;
            sd->segment_offset = cmdoptions.image_base_offset + sd->linear_offset - (segrel << 4ul);

            if ((sd->segment_offset+sd->segment_length) > 0xFFFFul) {
                dump_link_segments(DUMPLS_LINEAR);
                fprintf(stderr,"EXE: segment offset out of range\n");
                return -1;
            }
        }
    }
    else if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_DOSDRV) {
        unsigned int linkseg;

        for (linkseg=0;linkseg < link_segments.size();linkseg++) {
            shared_ptr<struct link_segdef> sd = link_segments[linkseg];

            sd->segment_base = cmdoptions.image_base_offset;
            sd->segment_relative = cmdoptions.image_base_segment;
            sd->segment_reloc_adj = cmdoptions.image_base_segment_reloc_adjust;
            sd->segment_offset = cmdoptions.image_base_offset + sd->linear_offset;

            if ((sd->segment_offset+sd->segment_length) > 0xFFFFul) {
                dump_link_segments(DUMPLS_LINEAR);
                fprintf(stderr,"COM: segment offset out of range\n");
                return -1;
            }
        }
    }

    return 0;
}

int fragment_def_arrange(struct link_segdef *sd) {
    segmentOffset ofs = 0;
    size_t fi;

    for (fi=0;fi < sd->fragments.size();fi++) {
        shared_ptr<struct seg_fragment> frag = sd->fragments[fi];

        /* NTS: mask = 0xFFFF           ~mask = 0x0000      alignment = 1 (0 + 1)
         *      mask = 0xFFFE           ~mask = 0x0001      alignment = 2 (1 + 1)
         *      mask = 0xFFFC           ~mask = 0x0003      alignment = 4 (3 + 1)
         *      mask = 0xFFF8           ~mask = 0x0007      alignment = 8 (7 + 1)
         *      and so on */
        if (ofs & (~frag->fragment_alignment))
            ofs = (ofs | (~frag->fragment_alignment)) + (segmentOffset)1u;

        frag->offset = ofs;
        ofs += frag->fragment_length;
    }

    sd->segment_length = ofs;
    return 0;
}

int fragment_def_arrange(void) {
    size_t inf;

    for (inf=0;inf < link_segments.size();inf++)
        fragment_def_arrange(link_segments[inf].get());

    return 0;
}

int segment_def_arrange(void) {
    segmentOffset ofs = 0;
    size_t inf;

    for (inf=0;inf < link_segments.size();inf++) {
        shared_ptr<struct link_segdef> sd = link_segments[inf];

        /* NTS: mask = 0xFFFF           ~mask = 0x0000      alignment = 1 (0 + 1)
         *      mask = 0xFFFE           ~mask = 0x0001      alignment = 2 (1 + 1)
         *      mask = 0xFFFC           ~mask = 0x0003      alignment = 4 (3 + 1)
         *      mask = 0xFFF8           ~mask = 0x0007      alignment = 8 (7 + 1)
         *      and so on */
        if (ofs & (~sd->segment_alignment))
            ofs = (ofs | (~sd->segment_alignment)) + (segmentOffset)1u;

        if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_EXE ||
            cmdoptions.output_format == OFMT_DOSDRV || cmdoptions.output_format == OFMT_DOSDRVEXE) {
            if (sd->segment_length > 0x10000ul) {
                dump_link_segments(DUMPLS_LINEAR);
                fprintf(stderr,"Segment too large >= 64KB\n");
                return -1;
            }
        }

        sd->linear_offset = ofs;
        ofs += sd->segment_length;
    }

    return 0;
}

void linkseg_add_padding_fragments(struct link_segdef *sg) {
    vector< shared_ptr<seg_fragment> >::iterator scan;
    vector< shared_ptr<seg_fragment> > add;
    shared_ptr<seg_fragment> frag;
    segmentOffset expect = 0;

    scan = sg->fragments.begin();
    if (scan == sg->fragments.end()) return;
    frag = *scan;
    expect = frag->offset + frag->fragment_length;
    scan++;
    while (scan != sg->fragments.end()) {
        frag = *scan;
        if (expect > frag->offset) {
            fprintf(stderr,"ERROR: fragment overlap detected\n");
            break;
        }
        if (expect < frag->offset) {
            const segmentOffset gap = frag->offset - expect;
            shared_ptr<seg_fragment> nf(new seg_fragment);

            nf->in_file = in_fileRefPadding;
            nf->offset = expect;
            nf->fragment_length = gap;
            nf->fragment_alignment = byteAlignMask;
            nf->attr = frag->attr;

            nf->image.resize(nf->fragment_length);

            if (sg->classname == "CODE")
                memset(&nf->image[0],0x90/*NOP*/,nf->fragment_length);
            else
                memset(&nf->image[0],0x00,nf->fragment_length);

            add.push_back(move(nf));
        }

        expect = frag->offset + frag->fragment_length;
        scan++;
    }

    if (!add.empty()) {
        for (scan=add.begin();scan!=add.end();scan++)
            sg->fragments.push_back(move(*scan));

        sort(sg->fragments.begin(), sg->fragments.end(), link_segments_qsort_frag_by_offset);
    }
}

int main(int argc,char **argv) {
    string hex_output_tmpfile;
    unsigned char diddump = 0;
    string hex_output_name;
    unsigned char pass;
    int i,fd,ret;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                char *s = argv[i++];
                if (s == NULL) return 1;

                input_file ent;

                ent.path = s; /* constructs std::string from char* */
                ent.segment_group = cmdoptions.next_segment_group;

                cmdoptions.in_file.push_back(ent);
            }
            else if (!strcmp(a,"hsym")) {
                a = argv[i++];
                if (a == NULL) return 1;
                cmdoptions.dosdrv_header_symbol = a;
            }
            else if (!strcmp(a,"pflat")) {
                cmdoptions.prefer_flat = 1;
            }
            else if (!strncmp(a,"stack",5)) {
                a += 5;
                if (!isxdigit(*a)) return 1;
                cmdoptions.want_stack_size = strtoul(a,NULL,16);
            }
            else if (!strncmp(a,"com",3)) {
                a += 3;
                if (!isxdigit(*a)) return 1;
                cmdoptions.image_base_offset = strtoul(a,NULL,16);
            }
            else if (!strcmp(a,"hex")) {
                a = argv[i++];
                if (a == NULL) return 1;
                cmdoptions.hex_output = a;
            }
            else if (!strcmp(a,"of")) {
                a = argv[i++];
                if (a == NULL) return 1;

                if (!strcmp(a,"com"))
                    cmdoptions.output_format = OFMT_COM;
                else if (!strcmp(a,"comrel")) {
                    cmdoptions.output_format = OFMT_COM;
                    cmdoptions.output_format_variant = OFMTVAR_COMREL;
                }
                else if (!strcmp(a,"exe"))
                    cmdoptions.output_format = OFMT_EXE;
                else if (!strcmp(a,"dosdrv"))
                    cmdoptions.output_format = OFMT_DOSDRV;
                else if (!strcmp(a,"dosdrvrel")) {
                    cmdoptions.output_format = OFMT_DOSDRV;
                    cmdoptions.output_format_variant = OFMTVAR_COMREL;
                }
                else if (!strcmp(a,"dosdrvexe"))
                    cmdoptions.output_format = OFMT_DOSDRVEXE;
                else {
                    fprintf(stderr,"Unknown format\n");
                    return 1;
                }
            }
            else if (!strcmp(a,"map")) {
                char *s = argv[i++];
                if (s == NULL) return 1;
                cmdoptions.map_file = s;
            }
            else if (!strcmp(a,"o")) {
                char *s = argv[i++];
                if (s == NULL) return 1;
                cmdoptions.out_file = s;
            }
            else if (!strcmp(a,"v")) {
                cmdoptions.verbose = true;
            }
            else if (!strcmp(a,"seggroup")) {
                cmdoptions.next_segment_group++;
            }
            else if (!strcmp(a,"dosseg")) {
                cmdoptions.do_dosseg = true;
            }
            else if (!strcmp(a,"no-dosseg")) {
                cmdoptions.do_dosseg = false;
            }
            else {
                help();
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    if (cmdoptions.image_base_offset == segmentOffsetUndef) {
        if (cmdoptions.output_format == OFMT_COM) {
            cmdoptions.image_base_offset = 0x100;
        }
        else if (cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRV || cmdoptions.output_format == OFMT_DOSDRVEXE) {
            cmdoptions.image_base_offset = 0;
        }
        else {
            cmdoptions.image_base_offset = 0;
        }
    }

    if (cmdoptions.image_base_segment == segmentBaseUndef) {
        if (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRV || cmdoptions.output_format == OFMT_DOSDRVEXE) {
            if (cmdoptions.image_base_offset & 0xFul) {
                fprintf(stderr,"ERROR: image base offset not a multiple of 16, and unspecified image base segment\n");
                return 1;
            }

            cmdoptions.image_base_segment = (segmentBase)(-(cmdoptions.image_base_offset >> (segmentOffset)4ul));
        }
        else {
            cmdoptions.image_base_segment = 0;
        }
    }

    if (cmdoptions.image_base_segment_reloc_adjust == segmentBaseUndef) {
        if (cmdoptions.output_format_variant == OFMTVAR_COMREL && (cmdoptions.output_format == OFMT_COM || cmdoptions.output_format == OFMT_DOSDRV)) {
            cmdoptions.image_base_segment_reloc_adjust = -cmdoptions.image_base_segment;
        }
        else {
            cmdoptions.image_base_segment_reloc_adjust = 0;
        }
    }

    if (!cmdoptions.map_file.empty()) {
        map_fp = fopen(cmdoptions.map_file.c_str(),"w");
        if (map_fp == NULL) return 1;
        setbuf(map_fp,NULL);
    }

    if (cmdoptions.in_file.empty()) { /* is the vector empty? */
        help();
        return 1;
    }

    if (cmdoptions.out_file.empty()) {
        help();
        return 1;
    }

    for (pass=0;pass < PASS_MAX;pass++) {
        for (current_in_file=0;current_in_file < cmdoptions.in_file.size();current_in_file++) {
            assert(!cmdoptions.in_file[current_in_file].path.empty());

            fd = open(cmdoptions.in_file[current_in_file].path.c_str(),O_RDONLY|O_BINARY);
            if (fd < 0) {
                fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
                return 1;
            }

            // prepare parsing
            if ((omf_state=omf_context_create()) == NULL) {
                fprintf(stderr,"Failed to init OMF parsing state\n");
                return 1;
            }
            omf_state->flags.verbose = (cmdoptions.verbose > 0);

            diddump = 0;
            current_in_file_module = 0;
            omf_context_begin_file(omf_state);
            current_segment_group = cmdoptions.in_file[current_in_file].segment_group;

            do {
                ret = omf_context_read_fd(omf_state,fd);
                if (ret == 0) {
                    if (apply_FIXUPP(omf_state,0,current_in_file,current_in_file_module,pass))
                        return 1;
                    omf_fixupps_context_free_entries(&omf_state->FIXUPPs);

                    for (auto li=link_segments.begin();li!=link_segments.end();li++) {
                        shared_ptr<struct link_segdef> sd = *li;
                        sd->fragment_load_index = fragmentRefUndef;
                    }

                    if (omf_record_is_modend(&omf_state->record)) {
                        if (!diddump && cmdoptions.verbose) {
                            my_dumpstate(omf_state);
                            diddump = 1;
                        }

                        if (cmdoptions.verbose)
                            printf("----- next module -----\n");

                        ret = omf_context_next_lib_module_fd(omf_state,fd);
                        if (ret < 0) {
                            printf("Unable to advance to next .LIB module, %s\n",strerror(errno));
                            if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
                        }
                        else if (ret > 0) {
                            current_in_file_module++;
                            omf_context_begin_module(omf_state);
                            diddump = 0;
                            continue;
                        }
                    }

                    break;
                }
                else if (ret < 0) {
                    fprintf(stderr,"Error: %s\n",strerror(errno));
                    if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
                    break;
                }

                switch (omf_state->record.rectype) {
                    case OMF_RECTYPE_EXTDEF:/*0x8C*/
                    case OMF_RECTYPE_LEXTDEF:/*0xB4*/
                    case OMF_RECTYPE_LEXTDEF32:/*0xB5*/
                        {
                            int first_new_extdef;

                            if ((first_new_extdef=omf_context_parse_EXTDEF(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing EXTDEF\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_EXTDEF(stdout,omf_state,(unsigned int)first_new_extdef);

                            // TODO: Store as symbol, noting it is external
                        } break;
                    case OMF_RECTYPE_PUBDEF:/*0x90*/
                    case OMF_RECTYPE_PUBDEF32:/*0x91*/
                    case OMF_RECTYPE_LPUBDEF:/*0xB6*/
                    case OMF_RECTYPE_LPUBDEF32:/*0xB7*/
                        {
                            int p_count = omf_state->PUBDEFs.omf_PUBDEFS_count;
                            int first_new_pubdef;

                            if ((first_new_pubdef=omf_context_parse_PUBDEF(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing PUBDEF\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_PUBDEF(stdout,omf_state,(unsigned int)first_new_pubdef);

                            /* TODO: LPUBDEF symbols need to "disappear" at the end of the module.
                             *       LPUBDEF means the symbols are not visible outside the module. */

                            if (pass == PASS_GATHER && pubdef_add(omf_state, p_count, omf_state->record.rectype, current_in_file, current_in_file_module, pass))
                                return 1;
                        } break;
                    case OMF_RECTYPE_LNAMES:/*0x96*/
                        {
                            int first_new_lname;

                            if ((first_new_lname=omf_context_parse_LNAMES(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing LNAMES\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_LNAMES(stdout,omf_state,(unsigned int)first_new_lname);

                        } break;
                    case OMF_RECTYPE_SEGDEF:/*0x98*/
                    case OMF_RECTYPE_SEGDEF32:/*0x99*/
                        {
                            int p_count = omf_state->SEGDEFs.omf_SEGDEFS_count;
                            int first_new_segdef;

                            if ((first_new_segdef=omf_context_parse_SEGDEF(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing SEGDEF\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_SEGDEF(stdout,omf_state,(unsigned int)first_new_segdef);

                            if (segdef_add(omf_state, p_count, current_in_file, current_in_file_module, pass))
                                return 1;
                        } break;
                    case OMF_RECTYPE_GRPDEF:/*0x9A*/
                    case OMF_RECTYPE_GRPDEF32:/*0x9B*/
                        {
                            int p_count = omf_state->GRPDEFs.omf_GRPDEFS_count;
                            int first_new_grpdef;

                            if ((first_new_grpdef=omf_context_parse_GRPDEF(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing GRPDEF\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_GRPDEF(stdout,omf_state,(unsigned int)first_new_grpdef);

                            if (pass == PASS_GATHER && grpdef_add(omf_state, p_count))
                                return 1;
                        } break;
                    case OMF_RECTYPE_FIXUPP:/*0x9C*/
                    case OMF_RECTYPE_FIXUPP32:/*0x9D*/
                        {
                            int first_new_fixupp;

                            if ((first_new_fixupp=omf_context_parse_FIXUPP(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing FIXUPP\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_FIXUPP(stdout,omf_state,(unsigned int)first_new_fixupp);
                        } break;
                    case OMF_RECTYPE_LEDATA:/*0xA0*/
                    case OMF_RECTYPE_LEDATA32:/*0xA1*/
                        {
                            struct omf_ledata_info_t info;

                            if (omf_context_parse_LEDATA(omf_state,&info,&omf_state->record) < 0) {
                                fprintf(stderr,"Error parsing LEDATA\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose && pass == PASS_GATHER)
                                dump_LEDATA(stdout,omf_state,&info);

                            if (pass == PASS_BUILD && ledata_add(omf_state, &info, current_in_file, current_in_file_module, pass))
                                return 1;
                        } break;
                    case OMF_RECTYPE_MODEND:/*0x8A*/
                    case OMF_RECTYPE_MODEND32:/*0x8B*/
                        if (pass == PASS_GATHER) {
                            unsigned char ModuleType;
                            unsigned char EndData;
                            unsigned int FrameDatum;
                            unsigned int TargetDatum;
                            unsigned long TargetDisplacement;
                            const struct omf_segdef_t *frame_segdef;
                            const struct omf_segdef_t *target_segdef;

                            ModuleType = omf_record_get_byte(&omf_state->record);
                            if (ModuleType&0x40/*START*/) {
                                EndData = omf_record_get_byte(&omf_state->record);
                                FrameDatum = omf_record_get_index(&omf_state->record);
                                TargetDatum = omf_record_get_index(&omf_state->record);

                                if (omf_state->record.rectype == OMF_RECTYPE_MODEND32)
                                    TargetDisplacement = omf_record_get_dword(&omf_state->record);
                                else
                                    TargetDisplacement = omf_record_get_word(&omf_state->record);

                                frame_segdef = omf_segdefs_context_get_segdef(&omf_state->SEGDEFs,FrameDatum);
                                target_segdef = omf_segdefs_context_get_segdef(&omf_state->SEGDEFs,TargetDatum);

                                if (cmdoptions.verbose) {
                                    printf("ModuleType: 0x%02x: MainModule=%u Start=%u Segment=%u StartReloc=%u\n",
                                            ModuleType,
                                            ModuleType&0x80?1:0,
                                            ModuleType&0x40?1:0,
                                            ModuleType&0x20?1:0,
                                            ModuleType&0x01?1:0);
                                    printf("    EndData=0x%02x FrameDatum=%u(%s) TargetDatum=%u(%s) TargetDisplacement=0x%lx\n",
                                            EndData,
                                            FrameDatum,
                                            (frame_segdef!=NULL)?omf_lnames_context_get_name_safe(&omf_state->LNAMEs,frame_segdef->segment_name_index):"",
                                            TargetDatum,
                                            (target_segdef!=NULL)?omf_lnames_context_get_name_safe(&omf_state->LNAMEs,target_segdef->segment_name_index):"",
                                            TargetDisplacement);
                                }

                                if (frame_segdef != NULL && target_segdef != NULL) {
                                    const char *framename = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,frame_segdef->segment_name_index);
                                    const char *targetname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,target_segdef->segment_name_index);

                                    if (cmdoptions.verbose)
                                        fprintf(stderr,"'%s' vs '%s'\n",framename,targetname);

                                    if (*framename != 0 && *targetname != 0) {
                                        shared_ptr<struct link_segdef> frameseg,targseg;

                                        targseg = find_link_segment(targetname);
                                        frameseg = find_link_segment(framename);
                                        if (targseg != NULL && frameseg != NULL) {
                                            entry_seg_ofs = TargetDisplacement;

                                            /* don't blindly assume TargetDisplacement is relative to the whole segment, it's relative to the
                                             * SEGDEF of the current object/module and we need to locate exactly that! */
                                            entry_seg_link_target_fragment =
                                                find_link_segment_by_file_module_and_segment_index(targseg.get(),current_in_file,current_in_file_module,TargetDatum);
                                            if (entry_seg_link_target_fragment == fragmentRefUndef) {
                                                fprintf(stderr,"Unable to locate entry point\n");
                                                return 1;
                                            }

                                            entry_seg_link_target = targseg;
                                            entry_seg_link_frame = frameseg;
                                        }
                                        else {
                                            fprintf(stderr,"Did not find segments\n");
                                        }
                                    }
                                    else {
                                        fprintf(stderr,"frame/target name not found\n");
                                    }
                                }
                                else {
                                    fprintf(stderr,"frame/target segdef not found\n");
                                }
                            }
                        } break;
 
                    default:
                        break;
                }
            } while (1);

            if (!diddump && cmdoptions.verbose) {
                my_dumpstate(omf_state);
                diddump = 1;
            }

            if (apply_FIXUPP(omf_state,0,current_in_file,current_in_file_module,pass))
                return 1;
            omf_fixupps_context_free_entries(&omf_state->FIXUPPs);

            omf_context_clear(omf_state);
            omf_state = omf_context_destroy(omf_state);

            for (auto li=link_segments.begin();li!=link_segments.end();li++) {
                shared_ptr<struct link_segdef> sd = *li;
                sd->fragment_load_index = fragmentRefUndef;
            }

            close(fd);
            current_segment_group = -1;
        }

        if (pass == PASS_GATHER) {
            owlink_default_sort_seg();

            if (cmdoptions.do_dosseg)
                owlink_dosseg_sort_order();

            {
                unsigned int i;

                for (i=0;i < link_segments.size();i++) {
                    shared_ptr<struct link_segdef> ssg = link_segments[i];

                    if (ssg->classname == "STACK" || ssg->classname == "BSS") {
                        ssg->noemit = 1;
                    }
                }
            }

            if (cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRVEXE) {
                /* NTS: segment_length has not been computed yet, count fragments */
                shared_ptr<struct link_segdef> stacksg;

                {
                    shared_ptr<struct link_segdef> ssg;
                    unsigned int i;

                    for (i=0;i < link_segments.size();i++) {
                        ssg = link_segments[i];

                        if (ssg->classname == "STACK")
                            stacksg = ssg;
                    }
                }

                if (stacksg != NULL) {
                    fragment_def_arrange(stacksg.get());
                    if (stacksg->segment_length < cmdoptions.want_stack_size) {
                        shared_ptr<struct seg_fragment> frag = alloc_link_segment_fragment(stacksg.get());
                        frag->offset = stacksg->segment_length;
                        frag->attr = stacksg->attr;
                        frag->fragment_length = cmdoptions.want_stack_size - stacksg->segment_length;
                        stacksg->segment_length += frag->fragment_length;
                        frag->in_file = in_fileRefInternal;
                    }
                }
            }

            {
                /* entry point checkup */
                if (cmdoptions.output_format == OFMT_DOSDRV) {
                    /* MS-DOS device drivers do NOT have an entry point */
                    if (entry_seg_link_target != nullptr) {
                        fprintf(stderr,"WARNING: MS-DOS device drivers, flat format (.SYS) should not have entry point.\n");
                        fprintf(stderr,"         Entry point provided by input OBJs will be ignored.\n");
                    }
                }
                else {
                    if (entry_seg_link_target == nullptr) {
                        fprintf(stderr,"WARNING: No entry point found\n");
                    }
                }
            }

            /* entry point cannot be 32-bit */
            if (cmdoptions.output_format == OFMT_DOSDRV) {
                /* nothing */
            }
            else if (entry_seg_link_target != nullptr) {
                shared_ptr<struct seg_fragment> frag = entry_seg_link_target_fragment;
                assert(frag != nullptr);

                if (frag->attr.f.f.use32) {
                    fprintf(stderr,"Entry point cannot be 32-bit\n");
                    return 1;
                }
            }

            owlink_stack_bss_arrange();
            if (trim_noemit())
                return 1;
            if (fragment_def_arrange())
                return 1;
            if (segment_def_arrange())
                return 1;
            if (segment_exe_arrange())
                return 1;

            /* MS-DOS device drivers: header symbol must exist at the start of the resident image.
             * For .COM files that is the start of the file. For .EXE files that is the first byte in the file
             * past the EXE header. When loaded into memory, it must be the first byte at the base of the image. */
            if (cmdoptions.output_format == OFMT_DOSDRV || cmdoptions.output_format == OFMT_DOSDRVEXE) {
                shared_ptr<struct link_symbol> ls = find_link_symbol(cmdoptions.dosdrv_header_symbol.c_str(),in_fileRefUndef,in_fileModuleRefUndef);

                if (ls == nullptr) {
                    fprintf(stderr,"WARNING: Unable to find symbol '%s' which is DOS driver header. See also -hsym option.\n",
                        cmdoptions.dosdrv_header_symbol.c_str());
                }
                else {
                    assert(ls->fragment != nullptr);
                    assert(ls->segref != nullptr);

                    if (ls->segref->segment_relative != cmdoptions.image_base_segment || ls->segref->segment_offset != cmdoptions.image_base_offset) {
                        fprintf(stderr,"WARNING: DOS driver header '%s' exists at segment/offset other than base of image, which will make the driver invalid.\n",
                                cmdoptions.dosdrv_header_symbol.c_str());
                    }
                    else {
                        /* if the segment exists at base, but the fragment does not, and the symbol exists at the base
                         * of a fragment. it may be possible to locate the fragment and shuffle it to the beginning of the image */
                        if (ls->fragment->offset != 0 && ls->offset == 0) {
                            auto i = find(ls->segref->fragments.begin(),ls->segref->fragments.end(),ls->fragment);
                            assert(i != ls->segref->fragments.end()); /* must find it somewhere! */

                            /* if not at the beginning, then move it to the beginning. for speed, just use swap()
                             * because removing/inserting in a vector takes linear time (based on number of elements).
                             * remember everything is shared_ptr so there is no risk of stale pointers here. */
                            if (i != ls->segref->fragments.begin()) {
                                swap(*(ls->segref->fragments.begin()),*i);
                                fragment_def_arrange(ls->segref.get());

                                fprintf(stderr,"DOS driver header '%s' moved to base of image.\n",
                                        cmdoptions.dosdrv_header_symbol.c_str());
                            }
                        }

                        if ((ls->fragment->offset+ls->offset) != 0) {
                            fprintf(stderr,"WARNING: DOS driver header '%s' exists at offset other than base of image, which will make the driver invalid.\n",
                                    cmdoptions.dosdrv_header_symbol.c_str());
                        }
                    }
                }
            }

            /* allocate in-memory copy of the segments */
            for (auto li=link_segments.begin();li!=link_segments.end();li++) {
                shared_ptr<struct link_segdef> sd = *li;

                if (!sd->noemit) {
                    for (auto fi=sd->fragments.begin();fi!=sd->fragments.end();fi++) {
                        shared_ptr<struct seg_fragment> frag = *fi;

                        if (frag->fragment_length != 0) {
                            frag->image.resize(frag->fragment_length);
                            memset(&frag->image[0],0,frag->fragment_length);
                        }
                    }
                }
            }

            /* symbol for entry point */
            if (entry_seg_link_target != nullptr) {
                /* make the original entry point a symbol, change entry point to new place */
                shared_ptr<struct link_symbol> ls = find_link_symbol("$$SOURCEENTRYPOINT",in_fileRefUndef,in_fileModuleRefUndef);
                if (ls != NULL)
                    return 1;

                ls = new_link_symbol("$$SOURCEENTRYPOINT");
                if (ls == NULL)
                    return 1;

                ls->segref = entry_seg_link_target;
                ls->groupdef = entry_seg_link_target->groupname;
                ls->offset = entry_seg_ofs;
                ls->fragment = entry_seg_link_target_fragment;
                ls->in_file = entry_seg_link_target_fragment.get()->in_file;
                ls->in_module = entry_seg_link_target_fragment.get()->in_module;
            }

            /* COM relocatable: Inject patch code and relocation table, put original entry point within, redirect entry point */
            if (cmdoptions.output_format == OFMT_COM && cmdoptions.output_format_variant == OFMTVAR_COMREL && !exe_relocation_table.empty()) {
                shared_ptr<struct link_segdef> exeseg;
                uint32_t init_ip;

                /* I'm not going to make up a guess that won't work. Give me an entry point! */
                if (entry_seg_link_target == nullptr) {
                    fprintf(stderr,"COMREL link target requires entry point\n");
                    return 1;
                }

                {
                    shared_ptr<struct seg_fragment> frag = entry_seg_link_target_fragment;
                    assert(frag != nullptr);

                    if (entry_seg_link_target->segment_relative != cmdoptions.image_base_segment) {
                        fprintf(stderr,"COM entry point must reside in the same segment as everything else (%lx != %lx)\n",
                                (unsigned long)entry_seg_link_target->segment_relative,
                                (unsigned long)cmdoptions.image_base_segment);
                        return 1;
                    }

                    init_ip = entry_seg_ofs + entry_seg_link_target->segment_offset + frag->offset;
                }

                assert(init_ip >= cmdoptions.image_base_offset);

                exeseg = find_link_segment("__COMREL_INIT");
                if (exeseg != NULL)
                    return 1;

                exeseg = new_link_segment("__COMREL_INIT");
                if (exeseg == NULL)
                    return 1;

                exeseg->pinned = 1;
                exeseg->segment_group = ++cmdoptions.next_segment_group;
                exeseg->segment_reloc_adj = cmdoptions.image_base_segment_reloc_adjust;
                exeseg->segment_relative = cmdoptions.image_base_segment;
                exeseg->segment_base = cmdoptions.image_base_offset;

                {
                    /* make the original entry point a symbol, change entry point to new place */
                    shared_ptr<struct link_symbol> ls = find_link_symbol("__COMREL_INIT_ORIGINAL_ENTRY",in_fileRefUndef,in_fileModuleRefUndef);
                    if (ls != NULL)
                        return 1;

                    ls = new_link_symbol("__COMREL_INIT_ORIGINAL_ENTRY");
                    if (ls == NULL)
                        return 1;

                    ls->segref = entry_seg_link_target;
                    ls->groupdef = entry_seg_link_target->groupname;
                    ls->offset = entry_seg_ofs;
                    ls->fragment = entry_seg_link_target_fragment;
                    ls->in_file = entry_seg_link_target_fragment.get()->in_file;
                    ls->in_module = entry_seg_link_target_fragment.get()->in_module;
                }

                {
                    /* injected fixup code, wait until second pass to actually patch in values */
                    shared_ptr<struct seg_fragment> frag = alloc_link_segment_fragment(exeseg.get());
                    frag->in_file = in_fileRefInternal;
                    frag->fragment_length = sizeof(comrel_entry_point);
                    frag->image.resize(frag->fragment_length);
                    memcpy(&frag->image[0],comrel_entry_point,sizeof(comrel_entry_point));

                    /* replace entry point */
                    entry_seg_ofs = 0;
                    entry_seg_link_frame = exeseg;
                    entry_seg_link_target = exeseg;
                    entry_seg_link_target_fragment = frag;

                    {
                        /* make the original entry point a symbol, change entry point to new place */
                        shared_ptr<struct link_symbol> ls = find_link_symbol("__COMREL_INIT_RELOC_APPLY",in_fileRefUndef,in_fileModuleRefUndef);
                        if (ls != NULL)
                            return 1;

                        ls = new_link_symbol("__COMREL_INIT_RELOC_APPLY");
                        if (ls == NULL)
                            return 1;

                        ls->segref = exeseg;
                        ls->groupdef = exeseg->groupname;
                        ls->offset = 0;
                        ls->fragment = frag;
                        ls->in_file = in_fileRefInternal;
                        ls->in_module = in_fileModuleRefUndef;
                    }
                }

                {
                    shared_ptr<struct seg_fragment> frag = alloc_link_segment_fragment(exeseg.get());
                    frag->in_file = in_fileRefInternal;
                    frag->fragment_length = 2 * exe_relocation_table.size();
                    frag->image.resize(frag->fragment_length);

                    {
                        /* make the original entry point a symbol, change entry point to new place */
                        shared_ptr<struct link_symbol> ls = find_link_symbol("__COMREL_INIT_RELOC_TABLE",in_fileRefUndef,in_fileModuleRefUndef);
                        if (ls != NULL)
                            return 1;

                        ls = new_link_symbol("__COMREL_INIT_RELOC_TABLE");
                        if (ls == NULL)
                            return 1;

                        ls->segref = exeseg;
                        ls->groupdef = exeseg->groupname;
                        ls->offset = 0;
                        ls->fragment = frag;
                        ls->in_file = in_fileRefInternal;
                        ls->in_module = in_fileModuleRefUndef;
                    }
                }

                fragment_def_arrange(exeseg.get());
                owlink_stack_bss_arrange(); /* __COMREL_INIT is meant to attach to the end of the .COM image */
                if (segment_def_arrange())
                    return 1;
                if (segment_exe_arrange())
                    return 1;
            }

            /* COM files: if the entry point is anywhere other than the start of the image, insert a JMP instruction */
            if (cmdoptions.output_format == OFMT_COM) {
                if (entry_seg_link_target != nullptr) {
                    uint32_t init_ip;

                    {
                        shared_ptr<struct seg_fragment> frag = entry_seg_link_target_fragment;
                        assert(frag != nullptr);

                        if (entry_seg_link_target->segment_relative != cmdoptions.image_base_segment) {
                            fprintf(stderr,"COM entry point must reside in the same segment as everything else (%lx != %lx)\n",
                                    (unsigned long)entry_seg_link_target->segment_relative,
                                    (unsigned long)cmdoptions.image_base_segment);
                            return 1;
                        }

                        init_ip = entry_seg_ofs + entry_seg_link_target->segment_offset + frag->offset;
                    }

                    if (init_ip != cmdoptions.image_base_offset) {
                        shared_ptr<struct link_segdef> exeseg;

                        assert(init_ip > cmdoptions.image_base_offset);

                        fprintf(stderr,"COM entry point does not point to beginning of image, adding JMP instruction\n");

                        exeseg = find_link_segment("__COM_ENTRY_JMP");
                        if (exeseg != NULL)
                            return 1;

                        /* segment MUST exist at the beginning of the file! COM executables always run from the first byte at 0x100! */
                        exeseg = new_link_segment_begin("__COM_ENTRY_JMP");
                        if (exeseg == NULL)
                            return 1;

                        exeseg->pinned = 1;
                        exeseg->segment_reloc_adj = cmdoptions.image_base_segment_reloc_adjust;
                        exeseg->segment_relative = cmdoptions.image_base_segment;
                        exeseg->segment_base = cmdoptions.image_base_offset;

                        size_t fidx;
                        shared_ptr<struct seg_fragment> frag = alloc_link_segment_fragment(exeseg.get(),/*&*/fidx);
                        /* must be the FIRST in the segment! */
                        assert(fidx == 0);
                        frag->in_file = in_fileRefInternal;
                        frag->offset = 0;

                        if (init_ip >= (0x80+cmdoptions.image_base_offset)) {
                            /* 3 byte JMP */
                            frag->fragment_length = 3;
                            exeseg->segment_length += frag->fragment_length;
                            frag->image.resize(frag->fragment_length);

                            frag->image[0] = 0xE9; /* JMP near */
                            *((uint16_t*)(&frag->image[1])) = 0; /* patch later */
                        }
                        else {
                            /* 2 byte JMP */
                            frag->fragment_length = 2;
                            exeseg->segment_length += frag->fragment_length;
                            frag->image.resize(frag->fragment_length);

                            frag->image[0] = 0xEB; /* JMP short */
                            frag->image[1] = 0; /* patch later */
                        }

                        exeseg = find_link_segment("__COM_ENTRY_JMP");
                        if (exeseg == NULL)
                            return 1;

                        /* make the original entry point a symbol, change entry point to new place */
                        shared_ptr<struct link_symbol> ls = find_link_symbol("__COM_ENTRY_ORIGINAL_ENTRY",in_fileRefUndef,in_fileModuleRefUndef);
                        if (ls != NULL)
                            return 1;

                        ls = new_link_symbol("__COM_ENTRY_ORIGINAL_ENTRY");
                        if (ls == NULL)
                            return 1;

                        ls->segref = entry_seg_link_target;
                        ls->groupdef = entry_seg_link_target->groupname;
                        ls->offset = entry_seg_ofs;
                        ls->fragment = entry_seg_link_target_fragment;
                        ls->in_file = entry_seg_link_target_fragment.get()->in_file;
                        ls->in_module = entry_seg_link_target_fragment.get()->in_module;

                        /* the new entry point is the first byte of the .COM image */
                        entry_seg_ofs = 0;
                        entry_seg_link_frame = exeseg;
                        entry_seg_link_target = exeseg;
                        entry_seg_link_target_fragment = frag;

                        fragment_def_arrange(exeseg.get());
                        if (segment_def_arrange())
                            return 1;
                        if (segment_exe_arrange())
                            return 1;
                    }
                }
            }
        }
    }

    /* symbol for entry point */
    if (entry_seg_link_target != nullptr) {
        /* make the original entry point a symbol, change entry point to new place */
        shared_ptr<struct link_symbol> ls = find_link_symbol("$$ENTRYPOINT",in_fileRefUndef,in_fileModuleRefUndef);
        if (ls != NULL)
            return 1;

        ls = new_link_symbol("$$ENTRYPOINT");
        if (ls == NULL)
            return 1;

        ls->segref = entry_seg_link_target;
        ls->groupdef = entry_seg_link_target->groupname;
        ls->offset = entry_seg_ofs;
        ls->fragment = entry_seg_link_target_fragment;
        ls->in_file = entry_seg_link_target_fragment.get()->in_file;
        ls->in_module = entry_seg_link_target_fragment.get()->in_module;
    }

    /* add padding fragments where needed */
    {
        size_t li;

        for (li=0;li < link_segments.size();li++) {
            sort(link_segments[li].get()->fragments.begin(), link_segments[li].get()->fragments.end(), link_segments_qsort_frag_by_offset);
            linkseg_add_padding_fragments(link_segments[li].get());
        }
    }

    sort(link_segments.begin(), link_segments.end(), link_segments_qsort_by_linofs);

    dump_link_relocations();
    dump_link_symbols();
    dump_link_segments(DUMPLS_LINEAR);

    sort(link_symbols.begin(), link_symbols.end(), link_symbol_qsort_cmp);

    /* decide file offsets */
    {
        unsigned int linkseg;

        for (linkseg=0;linkseg < link_segments.size();linkseg++) {
            shared_ptr<struct link_segdef> sd = link_segments[linkseg];

            if (!sd->noemit)
                sd->file_offset = sd->linear_offset;
        }
    }

    /* COMREL files: patch in entry point, table */
    if (cmdoptions.output_format == OFMT_COM && cmdoptions.output_format_variant == OFMTVAR_COMREL) {
        shared_ptr<struct link_segdef> exeseg;
        uint32_t end_jmp_ip;
        uint32_t init_ip;
        uint32_t tbl_ip;

        /* Add segment to end of list. __COMREL_INIT should exist at the back of the file so it can easily
         * be discarded after running without harming the program we're linking together. */
        exeseg = find_link_segment("__COMREL_INIT");
        if (exeseg != NULL) {
            {
                shared_ptr<struct link_symbol> ls = find_link_symbol("__COMREL_INIT_ORIGINAL_ENTRY",in_fileRefInternal,in_fileModuleRefUndef);
                if (ls == NULL)
                    return 1;

                shared_ptr<struct link_segdef> lsseg = ls->segref;
                shared_ptr<struct seg_fragment> lsfrag = ls->fragment;

                init_ip = ls->offset + lsseg->segment_offset + lsfrag->offset;
                assert(init_ip >= cmdoptions.image_base_offset);
            }

            shared_ptr<struct seg_fragment> apply_frag;
            {
                shared_ptr<struct link_symbol> apply_ls = find_link_symbol("__COMREL_INIT_RELOC_APPLY",in_fileRefUndef,in_fileModuleRefUndef);
                if (apply_ls == NULL)
                    return 1;

                apply_frag = apply_ls->fragment;
                assert(apply_frag->image.size() >= sizeof(comrel_entry_point));

                end_jmp_ip = exeseg->segment_offset + apply_frag->offset + comrel_entry_point_JMP_ENTRY + 2; // first byte past JMP
            }

            shared_ptr<struct seg_fragment> table_frag;
            {
                shared_ptr<struct link_symbol> table_ls = find_link_symbol("__COMREL_INIT_RELOC_TABLE",in_fileRefUndef,in_fileModuleRefUndef);
                if (table_ls == NULL)
                    return 1;

                table_frag = table_ls->fragment;
                assert(table_frag->image.size() >= (2 * exe_relocation_table.size()));

                tbl_ip = exeseg->segment_offset + table_frag->offset;

                uint16_t *w = (uint16_t*)(&table_frag->image[0]);
                uint16_t *wf = (uint16_t*)(&table_frag->image[table_frag->image.size() & (~1u)]);

                for (auto i=exe_relocation_table.begin();i!=exe_relocation_table.end();i++) {
                    shared_ptr<struct exe_relocation> rel = *i;
                    shared_ptr<struct link_segdef> lsg = rel->segref;
                    shared_ptr<struct seg_fragment> frag = rel->fragment;

                    unsigned long roff = rel->offset + lsg->segment_offset + frag->offset;
                    if (roff >= 0xFFFEul) {
                        fprintf(stderr,"COMREL: Relocation out of range\n");
                        return 1;
                    }

                    *w++ = (uint16_t)roff;
                    assert(w <= wf);
                }
            }

            *((uint16_t*)(&apply_frag->image[comrel_entry_point_CX_COUNT])) = exe_relocation_table.size();
            *((uint16_t*)(&apply_frag->image[comrel_entry_point_SI_OFFSET])) = (uint16_t)tbl_ip;
            *((uint16_t*)(&apply_frag->image[comrel_entry_point_JMP_ENTRY])) = (uint16_t)((init_ip - end_jmp_ip) & 0xFFFFul);
        }
    }

    /* COM files: patch JMP instruction to entry point */
    if (cmdoptions.output_format == OFMT_COM) {
        shared_ptr<struct link_segdef> exeseg;
        uint32_t init_ip;

        exeseg = find_link_segment("__COM_ENTRY_JMP");
        if (exeseg != NULL) {
            shared_ptr<struct link_symbol> ls = find_link_symbol("__COM_ENTRY_ORIGINAL_ENTRY",in_fileRefInternal,in_fileModuleRefUndef);
            if (ls == NULL)
                return 1;

            shared_ptr<struct link_segdef> lsseg = ls->segref;
            shared_ptr<struct seg_fragment> lsfrag = ls->fragment;

            init_ip = ls->offset + lsseg->segment_offset + lsfrag->offset;
            assert(init_ip >= cmdoptions.image_base_offset);
            assert(exeseg->fragments.size() >= 1); /* first one should be the JMP */
            shared_ptr<struct seg_fragment> frag = exeseg->fragments[0];
            assert(frag->image.size() >= 2);

            if (frag->image[0] == 0xEB) { /* jmp short */
                signed long rel = (signed long)init_ip - (2 + cmdoptions.image_base_offset);
                if (rel < -0x80l || rel > 0x7Fl) {
                    fprintf(stderr,"ERROR: JMP patch impossible, out of range %ld\n",rel);
                    return 1;
                }
                frag->image[1] = (unsigned char)rel;
            }
            else if (frag->image[0] == 0xE9) { /* jmp near */
                assert(frag->image.size() >= 3);
                signed long rel = (signed long)init_ip - (3 + cmdoptions.image_base_offset);
                if (rel < -0x8000l || rel > 0x7FFFl) {
                    fprintf(stderr,"ERROR: JMP patch impossible, out of range %ld\n",rel);
                    return 1;
                }
                *((uint16_t*)(&frag->image[1])) = (uint16_t)rel;
            }
            else {
                abort();
            }
        }
    }

    /* EXE files: make header and relocation table */
    if (cmdoptions.output_format == OFMT_EXE || cmdoptions.output_format == OFMT_DOSDRVEXE) {
        shared_ptr<struct link_segdef> exeseg;
        segmentOffset reloc_offset=0;
        uint16_t init_cs=0,init_ip=0;
        uint16_t init_ss=0,init_sp=0;
        uint32_t bss_segment_size=0;
        uint32_t stack_size=0;
        uint32_t tmp;

        exeseg = find_link_segment("$$EXEHEADER");
        if (exeseg != NULL)
            return 1;

        exeseg = new_link_segment("$$EXEHEADER");
        if (exeseg == NULL)
            return 1;

        exeseg->header = 1;
        exeseg->file_offset = 0;
        exeseg->segment_relative = -1;

        shared_ptr<struct seg_fragment> frag = alloc_link_segment_fragment(exeseg.get());

        /* TODO: If generating NE/PE/etc. set to 32, to make room for a DWORD at 0x1C that
         *       points at the NE/PE/LX/LE/etc header. */
        frag->fragment_length = 28;

        /* EXE relocation table is 4 bytes per entry, two WORDs containing ( offset, segment ) of an
         * address in the image to patch with the base segment */
        if (!exe_relocation_table.empty()) {
            reloc_offset = (segmentOffset)frag->fragment_length;
            frag->fragment_length += 4 * exe_relocation_table.size();
        }

        frag->in_file = in_fileRefInternal;
        frag->offset = exeseg->segment_length;
        exeseg->segment_length += frag->fragment_length;
        frag->image.resize(frag->fragment_length);

        linearAddress max_image = 0;
        linearAddress max_resident = 0;
        {
            unsigned int linkseg;

            for (linkseg=0;linkseg < link_segments.size();linkseg++) {
                const struct link_segdef *sd = link_segments[linkseg].get();
                const linearAddress m = sd->linear_offset + (linearAddress)sd->segment_length;

                if (sd->header) continue;

                if (max_resident < m) max_resident = m;
                if (!sd->noemit && max_image < m) max_image = m;
            }
        }

        if (max_image < max_resident)
            bss_segment_size = max_resident - max_image;

        if (bss_segment_size >= 0xFFFF0ul) {
            fprintf(stderr,"BSS/STACK too large for EXE\n");
            return 1;
        }

        {
            shared_ptr<struct link_segdef> stacksg = find_link_segment_by_class_last("STACK");

            if (stacksg != NULL) {
                init_ss = stacksg->segment_relative;
                init_sp = stacksg->segment_offset + stacksg->segment_length;
                stack_size = stacksg->segment_length;
            }
            else {
                fprintf(stderr,"WARNING: EXE without stack segment\n");
            }
        }

        if (entry_seg_link_target != nullptr && entry_seg_link_frame != nullptr) {
            shared_ptr<struct seg_fragment> frag = entry_seg_link_target_fragment;
            assert(frag != nullptr);

            if (entry_seg_link_target->segment_base != entry_seg_link_frame->segment_base) {
                fprintf(stderr,"EXE Entry point with frame != target not yet supported\n");
                return 1;
            }

            init_cs = entry_seg_link_target->segment_relative;
            init_ip = entry_seg_ofs + entry_seg_link_target->segment_offset + frag->offset;
        }
        else {
            fprintf(stderr,"WARNING: EXE without an entry point\n");
        }

        assert(reloc_offset < 0xFFF0u);

        {
            assert(frag->image.size() >= 28);
            unsigned char *ptr = &frag->image[0];

            memset(ptr,0,frag->image.size());

            ptr[0] = 'M';
            ptr[1] = 'Z';

            /* +2 = number of bytes in last block. if nonzero, only that much is used, else if zero, entire last block is used
             * +4 = number of blocks in EXE, including, if +2 nonzero, the partial last block */
            if (max_image & 511ul) { /* not a multiple of 512 */
                *((uint16_t*)(ptr+2)) = max_image & 511ul;
                *((uint16_t*)(ptr+4)) = (max_image >> 9ul) + 1ul;
            }
            else { /* multiple of 512 */
                *((uint16_t*)(ptr+2)) = 0ul;
                *((uint16_t*)(ptr+4)) = max_image >> 9ul;
            }

            /* relocation table */
            *((uint16_t*)(ptr+6)) = (uint16_t)exe_relocation_table.size(); /* count */
            *((uint16_t*)(ptr+24)) = (uint16_t)reloc_offset; /* offset */

            *((uint16_t*)(ptr+8)) = (frag->image.size()+0xFul) >> 4ul; /* size of header in paragraphs */

            /* number of paragraphs of additional memory needed */
            tmp = (uint32_t)((bss_segment_size+0xFul) >> 4ul);
            if (tmp > 0xFFFFul) tmp = 0xFFFFul;
            *((uint16_t*)(ptr+10)) = (uint16_t)tmp;

            /* maximum of paragraphs of additional memory */
            *((uint16_t*)(ptr+12)) = 0xFFFFul;

            /* stack pointer */
            *((uint16_t*)(ptr+14)) = init_ss;
            *((uint16_t*)(ptr+16)) = init_sp;

            /* checksum */
            *((uint16_t*)(ptr+18)) = 0;

            /* entry point (CS:IP) */
            *((uint16_t*)(ptr+20)) = init_ip;
            *((uint16_t*)(ptr+22)) = init_cs;
        }

        if (!exe_relocation_table.empty()) {
            assert(frag->image.size() >= (reloc_offset+(4*exe_relocation_table.size())));
            unsigned char *ptr = &frag->image[reloc_offset];
            size_t i;

            for (i=0;i < exe_relocation_table.size();i++) {
                shared_ptr<struct exe_relocation> rel = exe_relocation_table[i];
                shared_ptr<struct seg_fragment> frag = rel->fragment;
                shared_ptr<struct link_segdef> lsg = rel->segref;
                unsigned long rseg,roff;

                rseg = 0;
                roff = rel->offset + lsg->linear_offset + frag->offset;

                while (roff >= 0x4000ul) {
                    rseg += 0x10ul;
                    roff -= 0x100ul;
                }

                *((uint16_t*)(ptr + 0)) = (uint16_t)roff;
                *((uint16_t*)(ptr + 2)) = (uint16_t)rseg;
                ptr += 4;
            }
        }

        /* The EXE header is defined only in number of pages, round up */
        if (exeseg->segment_length & 0xF) {
            frag = alloc_link_segment_fragment(exeseg.get());
            frag->in_file = in_fileRefPadding;
            frag->offset = exeseg->segment_length;
            frag->fragment_length = 0x10 - (exeseg->segment_length & 0xF);
            exeseg->segment_length += frag->fragment_length;
            frag->image.resize(frag->fragment_length);
        }

        fileOffset header_size = exeseg->segment_length;

        if (map_fp != NULL) {
            fprintf(map_fp,"\n");

            fprintf(map_fp,"EXE header stats:\n");
            fprintf(map_fp,"---------------------------------------\n");

            fprintf(map_fp,"EXE header:                    0x%lx\n",(unsigned long)header_size);
            fprintf(map_fp,"EXE resident size with header: 0x%lx\n",(unsigned long)max_resident + (unsigned long)header_size);
            fprintf(map_fp,"EXE resident size:             0x%lx\n",(unsigned long)max_resident);
            fprintf(map_fp,"EXE disk size without header:  0x%lx\n",(unsigned long)max_image);
            fprintf(map_fp,"EXE disk size:                 0x%lx\n",(unsigned long)max_image + (unsigned long)header_size);
            fprintf(map_fp,"EXE stack size:                0x%lx\n",(unsigned long)stack_size);
            fprintf(map_fp,"EXE stack pointer:             %04lx:%04lx [0x%08lx]\n",
                    (unsigned long)init_ss,(unsigned long)init_sp,(unsigned long)((init_ss << 4ul) + init_sp));
            fprintf(map_fp,"EXE entry point:               %04lx:%04lx [0x%08lx]\n",
                    (unsigned long)init_cs,(unsigned long)init_ip,(unsigned long)((init_cs << 4ul) + init_ip));

            fprintf(map_fp,"\n");
        }

        /* shift file offsets */
        {
            unsigned int linkseg;

            for (linkseg=0;linkseg < link_segments.size();linkseg++) {
                shared_ptr<struct link_segdef> sd = link_segments[linkseg];

                if (!sd->noemit && !sd->header && sd->file_offset != fileOffsetUndef)
                    sd->file_offset += (fileOffset)header_size;
            }
        }
    }

    /* write output */
    sort(link_segments.begin(), link_segments.end(), link_segments_qsort_by_fileofs);
    dump_link_segments(DUMPLS_FILEOFFSET);
    assert(!cmdoptions.out_file.empty());
    {
        int fd;

        fd = open(cmdoptions.out_file.c_str(),O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
        if (fd < 0) {
            fprintf(stderr,"Unable to open output file\n");
            return 1;
        }

        {
            fileOffset cur_offset = 0;
            unsigned char fill[4096];

            for (auto li=link_segments.begin();li!=link_segments.end();li++) {
                shared_ptr<struct link_segdef> sd = *li;

                if (sd->noemit || sd->segment_length == 0) continue;

                assert(sd->file_offset != fileOffsetUndef);
                for (auto fi=sd->fragments.begin();fi!=sd->fragments.end();fi++) {
                    shared_ptr<struct seg_fragment> frag = *fi;
                    if (frag->fragment_length == 0) continue;

                    fileOffset file_ofs = sd->file_offset+frag->offset;

                    if (file_ofs < cur_offset) {
                        fprintf(stderr,"Fragment overlap error\n");
                        return 1;
                    }
                    else if (file_ofs > cur_offset) {
                        fileOffset gapsize = file_ofs - cur_offset;
                        if (gapsize > (fileOffset)(1024*1024)) {
                            fprintf(stderr,"Gap is too large (%lu)\n",(unsigned long)gapsize);
                            return 1;
                        }

                        memset(fill,0,sizeof(fill));
                        while (gapsize >= sizeof(fill)) {
                            if (write(fd,fill,sizeof(fill)) != sizeof(fill)) {
                                fprintf(stderr,"Write error\n");
                                return 1;
                            }
                            cur_offset += sizeof(fill);
                            gapsize -= sizeof(fill);
                        }
                        if (gapsize > (size_t)0) {
                            if (write(fd,fill,(size_t)gapsize) != (ssize_t)gapsize) {
                                fprintf(stderr,"Write error\n");
                                return 1;
                            }
                            cur_offset += (size_t)gapsize;
                            gapsize = 0;
                        }

                        assert(cur_offset == file_ofs);
                    }

                    assert(frag->image.size() == frag->fragment_length);
                    if ((unsigned long)write(fd,&frag->image[0],frag->fragment_length) != frag->fragment_length) {
                        fprintf(stderr,"Write error\n");
                        return 1;
                    }

                    cur_offset += frag->fragment_length;
                }
            }
        }

        close(fd);
    }

    if (map_fp != NULL) {
        fprintf(map_fp,"\n");
        fprintf(map_fp,"Entry point:\n");
        fprintf(map_fp,"---------------------------------------\n");

        if (entry_seg_link_target != nullptr) {
            auto frag = entry_seg_link_target_fragment;
            assert(frag != nullptr);

            shared_ptr<struct link_symbol> sym;
            unsigned long sofs,cofs;

            fprintf(map_fp,"  %04lx:%08lx %20s + 0x%08lx '%s'",
                (unsigned long)entry_seg_link_target->segment_relative&0xfffful,
                (unsigned long)entry_seg_link_target->segment_offset + (unsigned long)frag->offset + (unsigned long)entry_seg_ofs,
                entry_seg_link_target->name.c_str(),
                (unsigned long)frag->offset + (unsigned long)entry_seg_ofs,
                get_in_file(frag->in_file));

            if (frag->in_module != in_fileModuleRefUndef)
                fprintf(map_fp,":%u",frag->in_module);

            fprintf(map_fp,"\n");

            for (auto i=link_symbols.begin();i!=link_symbols.end();i++) {
                auto cur_sym = *i;

                if (cur_sym->name == "$$ENTRYPOINT") continue;
                if (cur_sym->name == "$$SOURCEENTRYPOINT") continue;
                if (cur_sym->segref != entry_seg_link_target) continue;

                shared_ptr<struct seg_fragment> sfrag = cur_sym->fragment;
                shared_ptr<struct link_segdef> ssg = cur_sym->segref;

                sofs = ssg->segment_offset + sfrag->offset + cur_sym->offset;
                cofs = entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs;

                if (sofs > cofs)
                    break;

                sym = cur_sym;
            }

            if (sym != nullptr) {
                assert(sym->segref == entry_seg_link_target);

                shared_ptr<struct seg_fragment> sfrag = sym->fragment;
                shared_ptr<struct link_segdef> ssg = sym->segref;

                sofs = ssg->segment_offset + sfrag->offset + sym->offset;
                cofs = entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs;

                fprintf(map_fp,"    %s + 0x%08lx\n",sym->name.c_str(),cofs - sofs);
            }
        }
        else {
            fprintf(map_fp,"  No entry point defined\n");
        }

        fprintf(map_fp,"\n");
    }

    if (map_fp != NULL) {
        fclose(map_fp);
        map_fp = NULL;
    }

    free_link_segments();
    free_exe_relocations();
    return 0;
}

