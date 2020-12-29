
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

//================================== PROGRAM ================================

/* <file ref> <module index ref>
 *
 * for .obj files, file ref is an index and module index ref is zero.
 * for .lib files, file ref is an index and module index is an index into the embedded .obj files within */

typedef size_t                          in_fileRef;             /* index into in_file */
typedef uint32_t                        in_fileModuleRef;       /* within a file ref */
typedef uint32_t                        segmentSize;
typedef uint32_t                        segmentBase;

static const in_fileRef                 in_fileRefUndef = ~((in_fileRef)0u);
static const in_fileRef                 in_fileRefInternal = in_fileRefUndef - (in_fileRef)1u;
static const in_fileModuleRef           in_fileModuleRefUndef = ~((in_fileModuleRef)0u);
static const segmentSize                segmentSizeUndef = ~((segmentSize)0u);
static const segmentBase                segmentBaseUndef = ~((segmentBase)0u);

#define MAX_GROUPS                      256


static FILE*                            map_fp = NULL;

struct cmdoptions {
    unsigned int                        hex_split:1;
    unsigned int                        hex_cpp:1;
    unsigned int                        do_dosseg:1;
    unsigned int                        verbose:1;

    segmentSize                         want_stack_size;

/* NTS: Default -com100, use -com0 for Open Watcom compiled C source */
    segmentBase                         com_segbase;

    string                              dosdrv_header_symbol;
    string                              hex_output;
    string                              out_file;
    string                              map_file;

    vector<string>                      in_file;

    cmdoptions() : hex_split(false), hex_cpp(false), do_dosseg(true), verbose(false), want_stack_size(4096),
                   com_segbase(segmentBaseUndef), dosdrv_header_symbol("_dosdrv_header") { }
};

static cmdoptions                       cmdoptions;

static in_fileRef                       current_in_file = 0;
static in_fileModuleRef                 current_in_file_module = 0;

const char *get_in_file(const in_fileRef idx) {
    if (idx == in_fileRefUndef)
        return "<undefined>";
    else if (idx == in_fileRefInternal)
        return "<internal>";
    else if (idx < cmdoptions.in_file.size()) {
        if (!cmdoptions.in_file[idx].empty())
            return cmdoptions.in_file[idx].c_str();
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

struct exe_relocation {
    string                              segname;
    unsigned int                        fragment;
    unsigned long                       offset;

    exe_relocation() : fragment(~0u), offset(0) { }
};

static vector<struct exe_relocation>    exe_relocation_table;

struct exe_relocation *new_exe_relocation(void) {
    if (exe_relocation_table.empty())
        exe_relocation_table.reserve(4096);

    const size_t idx = exe_relocation_table.size();
    exe_relocation_table.resize(idx + (size_t)1);
    return &exe_relocation_table[idx];
}

void free_exe_relocations(void) {
    exe_relocation_table.clear();
}

static unsigned int                     output_format = OFMT_COM;
static unsigned int                     output_format_variant = OFMTVAR_NONE;

#define MAX_SEG_FRAGMENTS               1024

#if TARGET_MSDOS == 32 || defined(LINUX)
#define MAX_SYMBOLS                     65536
#else
#define MAX_SYMBOLS                     4096
#endif

struct link_symbol {
    char*                               name;
    char*                               segdef;
    char*                               groupdef;
    unsigned long                       offset;
    unsigned short                      fragment;
    in_fileRef                          in_file;
    in_fileModuleRef                    in_module;
    unsigned int                        is_local:1;
};

static struct link_symbol*              link_symbols = NULL;
static size_t                           link_symbols_count = 0;
static size_t                           link_symbols_alloc = 0;
static size_t                           link_symbols_nextalloc = 0;

int link_symbols_extend(size_t sz) {
    if (sz > MAX_SYMBOLS) return -1;
    if (sz <= link_symbols_alloc) return 0;
    if (sz == 0) return -1;

    if (link_symbols != NULL) {
        size_t old_alloc = link_symbols_alloc;
        struct link_symbol *n = (struct link_symbol*)realloc((void*)link_symbols, sz * sizeof(struct link_symbol));
        if (n == NULL) return -1;
        link_symbols = n;
        assert(old_alloc != 0);
        assert(old_alloc < sz);
        memset(link_symbols + old_alloc, 0, (sz - old_alloc) * sizeof(struct link_symbol));
    }
    else {
        link_symbols = (struct link_symbol*)malloc(sz * sizeof(struct link_symbol));
        if (link_symbols == NULL) return -1;
        memset(link_symbols, 0, sz * sizeof(struct link_symbol));
        link_symbols_nextalloc = 0;
        link_symbols_count = 0;
    }

    link_symbols_alloc = sz;
    return 0;
}

int link_symbols_extend_double(void) {
    if (link_symbols_alloc < MAX_SYMBOLS) {
        size_t ns = link_symbols_alloc * 2u;
        if (ns < 128) ns = 128;
        if (ns > MAX_SYMBOLS) ns = MAX_SYMBOLS;
        if (link_symbols_extend(ns)) return -1;
        assert(link_symbols_alloc >= ns);
    }

    return 0;
}

struct link_symbol *link_symbol_empty_slot(void) {
    struct link_symbol *sym = NULL;

    if (link_symbols == NULL && link_symbols_extend_double()) return NULL;

    do {
        assert(link_symbols_nextalloc <= link_symbols_count);
        assert(link_symbols_count <= link_symbols_alloc);
        while (link_symbols_nextalloc < link_symbols_count) {
            sym = link_symbols + (link_symbols_nextalloc++);
            if (sym->name == NULL) return sym;
        }
        while (link_symbols_count < link_symbols_alloc) {
            sym = link_symbols + (link_symbols_count++);
            if (sym->name == NULL) return sym;
        }

        if (link_symbols_extend_double()) return NULL;
    } while (1);

    return NULL;
}

struct link_symbol *new_link_symbol(const char *name) {
    struct link_symbol *sym = link_symbol_empty_slot();

    if (sym != NULL) {
        assert(sym->name == NULL);
        assert(sym->segdef == NULL);
        assert(sym->groupdef == NULL);

        sym->name = strdup(name);

        sym->in_file = in_fileRefUndef;
    }

    return sym;
}

struct link_symbol *find_link_symbol(const char *name,const in_fileRef in_file,const in_fileModuleRef in_module) {
    struct link_symbol *sym;
    size_t i = 0;

    if (link_symbols != NULL) {
        for (;i < link_symbols_count;i++) {
            sym = link_symbols + i;
            assert(sym->name != NULL);

            if (sym->is_local) {
                /* ignore local symbols unless file/module scope is given */
                if (in_file != in_fileRefUndef && sym->in_file != in_file)
                    continue;
                if (in_module != in_fileModuleRefUndef && sym->in_module != in_module)
                    continue;
            }

            if (!strcmp(sym->name, name))
                return sym;
        }
    }

    return NULL;
}

void link_symbol_free(struct link_symbol *s) {
    cstr_free(&(s->name));
    cstr_free(&(s->segdef));
    cstr_free(&(s->groupdef));
}

void link_symbols_free(void) {
    size_t i;

    if (link_symbols != NULL) {
        for (i=0;i < link_symbols_alloc;i++) link_symbol_free(&link_symbols[i]);
        free(link_symbols);
        link_symbols = NULL;
        link_symbols_alloc = 0;
        link_symbols_count = 0;
        link_symbols_nextalloc = 0;
    }
}

#define MAX_SEGMENTS                    256

struct seg_fragment {
    in_fileRef                          in_file;
    in_fileModuleRef                    in_module;
    unsigned short                      segidx;
    unsigned long                       offset;
    unsigned long                       fragment_length;
    struct omf_segdef_attr_t            attr;
};

struct link_segdef {
    struct omf_segdef_attr_t            attr;
    char*                               name;
    char*                               classname;
    char*                               groupname;
    unsigned long                       file_offset;
    unsigned long                       linear_offset;
    unsigned long                       segment_base;       /* segment base */
    unsigned long                       segment_offset;     /* offset within segment */
    unsigned long                       segment_length;     /* length in bytes */
    unsigned long                       segment_relative;   /* relative segment */
    unsigned short                      initial_alignment;
    unsigned long                       segment_len_count;
    unsigned long                       load_base;
    unsigned char*                      image_ptr;          /* size is segment_length */
    struct seg_fragment*                fragments;          /* fragments (one from each OBJ/module) */
    unsigned int                        fragments_count;
    unsigned int                        fragments_alloc;
    unsigned int                        fragments_read;
    unsigned char                       pinned;
    unsigned char                       noemit;
};

static struct link_segdef               link_segments[MAX_SEGMENTS];
static unsigned int                     link_segments_count = 0;

static struct link_segdef*              current_link_segment = NULL;

static unsigned int                     entry_seg_link_target_fragment = 0;
static char*                            entry_seg_link_target_name = NULL;
static struct link_segdef*              entry_seg_link_target = NULL;
static unsigned int                     entry_seg_link_frame_fragment = 0;
static char*                            entry_seg_link_frame_name = NULL;
static struct link_segdef*              entry_seg_link_frame = NULL;
static unsigned char                    com_entry_insert = 0;
static unsigned long                    entry_seg_ofs = 0;
static unsigned char                    prefer_flat = 0;

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
    if (a->groupname == NULL || strcmp(a->groupname,"DGROUP")) { /* not DGROUP */
        if (a->classname != NULL && strcmp(a->classname,"CODE") == 0) { /* CODE */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 2. return -1 if not DGROUP */
int sort_cmp_not_dgroup(const struct link_segdef *a) {
    if (a->groupname == NULL || strcmp(a->groupname,"DGROUP")) { /* not DGROUP */
        /* OK */
        return -1;
    }

    return 0;
}

/* 3. return -1 if group DGROUP, class BEGDATA */
int sort_cmp_dgroup_class_BEGDATA(const struct link_segdef *a) {
    if (a->groupname != NULL && strcmp(a->groupname,"DGROUP") == 0) { /* DGROUP */
        if (a->classname != NULL && strcmp(a->classname,"BEGDATA") == 0) { /* BEGDATA */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 4. return -1 if group DGROUP, not (class BEGDATA or class BSS or class STACK) */
int sort_cmp_dgroup_class_not_special(const struct link_segdef *a) {
    if (a->groupname != NULL && strcmp(a->groupname,"DGROUP") == 0) { /* DGROUP */
        if (a->classname != NULL && !(strcmp(a->classname,"BEGDATA") == 0 || strcmp(a->classname,"BSS") == 0 || strcmp(a->classname,"STACK") == 0)) { /* BEGDATA */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 5. return -1 if group DGROUP, class BSS */
int sort_cmp_dgroup_class_bss(const struct link_segdef *a) {
    if (a->groupname != NULL && strcmp(a->groupname,"DGROUP") == 0) { /* DGROUP */
        if (a->classname != NULL && strcmp(a->classname,"BSS") == 0) { /* BSS */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* 6. return -1 if group DGROUP, class STACK */
int sort_cmp_dgroup_class_stack(const struct link_segdef *a) {
    if (a->groupname != NULL && strcmp(a->groupname,"DGROUP") == 0) { /* DGROUP */
        if (a->classname != NULL && strcmp(a->classname,"STACK") == 0) { /* STACK */
            /* OK */
            return -1;
        }
    }

    return 0;
}

/* return 1 if class BSS */
int sort_cmpf_class_bss(const struct link_segdef *a) {
    if (a->classname != NULL && strcmp(a->classname,"BSS") == 0) { /* STACK */
        /* OK */
        return 1;
    }

    return 0;
}

/* return 1 if class STACK */
int sort_cmpf_class_stack(const struct link_segdef *a) {
    if (a->classname != NULL && strcmp(a->classname,"STACK") == 0) { /* STACK */
        /* OK */
        return 1;
    }

    return 0;
}

void link_segments_swap(unsigned int s1,unsigned int s2) {
    if (s1 != s2) {
        struct link_segdef t;

                        t = link_segments[s1];
        link_segments[s1] = link_segments[s2];
        link_segments[s2] = t;
    }
}

struct link_segdef *find_link_segment(const char *name);

void reconnect_gl_segs() {
    if (entry_seg_link_target_name) {
        entry_seg_link_target = find_link_segment(entry_seg_link_target_name);
        assert(entry_seg_link_target != NULL);
        assert(!strcmp(entry_seg_link_target->name,entry_seg_link_target_name));
    }
    if (entry_seg_link_frame_name) {
        entry_seg_link_frame = find_link_segment(entry_seg_link_frame_name);
        assert(entry_seg_link_frame != NULL);
        assert(!strcmp(entry_seg_link_frame->name,entry_seg_link_frame_name));
    }
}

void link_segments_sort(unsigned int *start,unsigned int *end,int (*sort_cmp)(const struct link_segdef *a)) {
    unsigned int i;
    int r;

    for (i=*start;i <= *end;) {
        r = sort_cmp(&link_segments[i]);
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

    reconnect_gl_segs();
}

void owlink_dosseg_sort_order(void) {
    unsigned int s = 0,e = link_segments_count - 1u;

    if (link_segments_count == 0) return;
    link_segments_sort(&s,&e,sort_cmp_not_dgroup_class_code);       /* 1 */
    link_segments_sort(&s,&e,sort_cmp_not_dgroup);                  /* 2 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_BEGDATA);        /* 3 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_not_special);    /* 4 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_bss);            /* 5 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_stack);          /* 6 */
}

int owlink_segsrt_def_qsort_cmp(const void *a,const void *b) {
    const struct link_segdef *sa = (const struct link_segdef*)a;
    const struct link_segdef *sb = (const struct link_segdef*)b;
    const char *gna,*gnb;
    const char *cna,*cnb;
//  const char *nna,*nnb;
    int ret = 0;

    /* if either one is pinned, don't move */
    if (sa->pinned || sb->pinned) return 0;

    /* sort by GROUP, CLASS, NAME */
    gna = sa->groupname ? sa->groupname : "";
    gnb = sb->groupname ? sb->groupname : "";
    cna = sa->classname ? sa->classname : "";
    cnb = sb->classname ? sb->classname : "";
//  nna = sa->name      ? sa->name      : "";
//  nnb = sb->name      ? sb->name      : "";

    /* do it */
                  ret = strcmp(gna,gnb);
    if (ret == 0) ret = strcmp(cna,cnb);
//  if (ret == 0) ret = strcmp(nna,nnb);

    return ret;
}

void owlink_default_sort_seg(void) {
    qsort(link_segments, link_segments_count, sizeof(struct link_segdef), owlink_segsrt_def_qsort_cmp);
    reconnect_gl_segs();
}

void owlink_stack_bss_arrange(void) {
    unsigned int s = 0,e = link_segments_count - 1u;

    if (link_segments_count == 0) return;
    if (output_format == OFMT_COM || output_format == OFMT_EXE || output_format == OFMT_DOSDRV || output_format == OFMT_DOSDRVEXE) {
        /* STACK and BSS must be placed at the end in BSS, STACK order */
        e = link_segments_count - 1u;
        s = 0;

        link_segments_sort(&s,&e,sort_cmpf_class_stack);
        link_segments_sort(&s,&e,sort_cmpf_class_bss);
    }
}

struct seg_fragment *alloc_link_segment_fragment(struct link_segdef *sg) {
    if (sg->fragments == NULL) {
        sg->fragments_alloc = 16;
        sg->fragments_count = 0;
        sg->fragments = (struct seg_fragment*)calloc(sg->fragments_alloc, sizeof(struct seg_fragment));
        if (sg->fragments == NULL) return NULL;
    }
    if (sg->fragments_count >= sg->fragments_alloc) {
        if (sg->fragments_alloc < MAX_SEG_FRAGMENTS) {
            unsigned int nalloc = sg->fragments_alloc * 2;
            struct seg_fragment *np;

            if (nalloc > MAX_SEG_FRAGMENTS)
                nalloc = MAX_SEG_FRAGMENTS;

            assert(sg->fragments_alloc < nalloc);

            np = (struct seg_fragment*)realloc((void*)sg->fragments, sizeof(struct seg_fragment) * nalloc);
            if (np == NULL) return NULL;
            memset(np + sg->fragments_alloc, 0, sizeof(struct seg_fragment) * (nalloc - sg->fragments_alloc));

            sg->fragments = np;
            sg->fragments_alloc = nalloc;
        }
        else {
            return NULL;
        }
    }

    assert(sg->fragments_count < sg->fragments_alloc);

    {
        struct seg_fragment *f = &sg->fragments[sg->fragments_count++];
        sg->fragments_read = sg->fragments_count;
        f->in_file = in_fileRefUndef;
        return f;
    }
}

void free_link_segment(struct link_segdef *sg) {
    cstr_free(&(sg->groupname));
    cstr_free(&(sg->classname));
    cstr_free(&(sg->name));

    if (sg->fragments != NULL) {
        free(sg->fragments);
        sg->fragments = NULL;
    }
    sg->fragments_alloc = 0;
    sg->fragments_count = 0;

    if (sg->image_ptr != NULL) {
        free(sg->image_ptr);
        sg->image_ptr = NULL;
    }
}

void free_link_segments(void) {
    while (link_segments_count > 0)
        free_link_segment(&link_segments[--link_segments_count]);
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
        struct exe_relocation *rel = &exe_relocation_table[i++];

        if (cmdoptions.verbose) {
            fprintf(stderr,"relocation[%u]: seg='%s' frag=%u offset=0x%lx\n",
                i,rel->segname.c_str(),rel->fragment,rel->offset);
        }

        if (map_fp != NULL) {
            struct seg_fragment *frag;
            struct link_segdef *sg;

            sg = find_link_segment(rel->segname.c_str());
            assert(sg != NULL);

            assert(sg->fragments != NULL);
            assert(sg->fragments_count <= sg->fragments_alloc);
            assert(rel->fragment < sg->fragments_count);
            frag = &sg->fragments[rel->fragment];

            fprintf(map_fp,"  %04lx:%08lx [0x%08lx] %20s + 0x%08lx from '%s':%u\n",
                sg->segment_relative&0xfffful,
                sg->segment_offset + frag->offset + rel->offset,
                sg->linear_offset + frag->offset + rel->offset,
                rel->segname.c_str(),frag->offset + rel->offset,
                get_in_file(frag->in_file),frag->in_module);
        }
    }

    if (map_fp != NULL)
        fprintf(map_fp,"\n");
}

int link_symbol_qsort_cmp_by_name(const void *a,const void *b) {
    const struct link_symbol *sa = (const struct link_symbol*)a;
    const struct link_symbol *sb = (const struct link_symbol*)b;

    assert(sa->name != NULL);
    assert(sb->name != NULL);

    return strcasecmp(sa->name,sb->name);
}

int link_symbol_qsort_cmp(const void *a,const void *b) {
    const struct link_symbol *sa = (const struct link_symbol*)a;
    const struct seg_fragment *fraga;
    const struct link_segdef *sga;

    const struct link_symbol *sb = (const struct link_symbol*)b;
    struct seg_fragment *fragb;
    struct link_segdef *sgb;

    unsigned long la,lb;

    /* -----A----- */
    assert(sa->segdef != NULL);

    sga = find_link_segment(sa->segdef);
    assert(sga != NULL);

    assert(sga->fragments != NULL);
    assert(sga->fragments_count <= sga->fragments_alloc);
    assert(sa->fragment < sga->fragments_count);
    fraga = &sga->fragments[sa->fragment];

    /* -----B----- */
    assert(sb->segdef != NULL);

    sgb = find_link_segment(sb->segdef);
    assert(sgb != NULL);

    assert(sgb->fragments != NULL);
    assert(sgb->fragments_count <= sgb->fragments_alloc);
    assert(sb->fragment < sgb->fragments_count);
    fragb = &sgb->fragments[sb->fragment];

    /* segment */
    la = sga->segment_relative;
    lb = sgb->segment_relative;

    if (la < lb) return -1;
    if (la > lb) return 1;

    /* offset */
    la = sga->segment_offset + fraga->offset + sa->offset;
    lb = sgb->segment_offset + fragb->offset + sb->offset;

    if (la < lb) return -1;
    if (la > lb) return 1;

    return 0;
}

void dump_hex_symbols(FILE *hfp,const char *symbol_name) {
    unsigned int i;

    if (link_symbols == NULL) return;

    i = 0;
    while (i < link_symbols_count) {
        struct link_symbol *sym = &link_symbols[i++];
        if (sym->name == NULL) continue;

        fprintf(hfp,"/*symbol[%u]: name='%s' group='%s' seg='%s' offset=0x%lx frag=%u file='%s' module=%u local=%u*/\n",
                i/*post-increment, intentional*/,sym->name,sym->groupdef,sym->segdef,sym->offset,sym->fragment,
                get_in_file(sym->in_file),sym->in_module,sym->is_local);

        {
            struct seg_fragment *frag;
            struct link_segdef *sg;

            assert(sym->segdef != NULL);

            sg = find_link_segment(sym->segdef);
            assert(sg != NULL);

            assert(sg->fragments != NULL);
            assert(sg->fragments_count <= sg->fragments_alloc);
            assert(sym->fragment < sg->fragments_count);
            frag = &sg->fragments[sym->fragment];

            fprintf(hfp,"/*  %-32s %c %04lx:%08lx [0x%08lx] %20s + 0x%08lx from '%s':%u*/\n",
                    sym->name,
                    sym->is_local?'L':'G',
                    sg->segment_relative&0xfffful,
                    sg->segment_offset + frag->offset + sym->offset,
                    sg->linear_offset + frag->offset + sym->offset,
                    sym->segdef,
                    frag->offset + sym->offset,
                    get_in_file(sym->in_file),
                    sym->in_module);

            if (!sg->noemit)
                fprintf(hfp,"#define %s_bin_symbol_%s_file_offset 0x%lxul /*offset in file*/\n",
                    symbol_name,sym->name,(unsigned long)sg->file_offset + frag->offset + sym->offset);

            fprintf(hfp,"#define %s_bin_symbol_%s_resident_offset 0x%lxul /*offset from base of resident image*/\n",
                    symbol_name,sym->name,(unsigned long)sg->linear_offset + frag->offset + sym->offset);

            fprintf(hfp,"#define %s_bin_symbol_%s_segment_relative 0x%lxul /*segment value relative to resident segment base*/\n",
                    symbol_name,sym->name,(unsigned long)sg->segment_relative);

            fprintf(hfp,"#define %s_bin_symbol_%s_segment_offset 0x%lxul /*offset from base of resident segment base*/\n",
                    symbol_name,sym->name,(unsigned long)((sg->segment_offset + frag->offset + sym->offset) - sg->segment_base));

            fprintf(hfp,"#define %s_bin_symbol_%s_segment_offset_with_base 0x%lxul /*offset with segment base offset added (that code would use)*/\n",
                    symbol_name,sym->name,(unsigned long)(sg->segment_offset + frag->offset + sym->offset));
        }
    }
}

void dump_link_symbols(void) {
    unsigned int i,pass=0,passes=1;

    if (link_symbols == NULL) return;

    if (map_fp != NULL)
        passes = 2;

    for (pass=0;pass < passes;pass++) {
        i = 0;

        if (map_fp != NULL) {
            fprintf(map_fp,"\n");
            fprintf(map_fp,"Symbol table %s: %u entries\n",pass == 0 ? "by name" : "by address",(unsigned int)link_symbols_count);
            fprintf(map_fp,"---------------------------------------\n");
        }

        if (cmdoptions.verbose || map_fp != NULL)
            qsort(link_symbols, link_symbols_count, sizeof(struct link_symbol),
                pass == 0 ? link_symbol_qsort_cmp_by_name : link_symbol_qsort_cmp);

        while (i < link_symbols_count) {
            struct link_symbol *sym = &link_symbols[i++];
            if (sym->name == NULL) continue;

            if (cmdoptions.verbose) {
                fprintf(stderr,"symbol[%u]: name='%s' group='%s' seg='%s' offset=0x%lx frag=%u file='%s' module=%u local=%u\n",
                        i/*post-increment, intentional*/,sym->name,sym->groupdef,sym->segdef,sym->offset,sym->fragment,
                        get_in_file(sym->in_file),sym->in_module,sym->is_local);
            }

            if (map_fp != NULL) {
                struct seg_fragment *frag;
                struct link_segdef *sg;

                assert(sym->segdef != NULL);

                sg = find_link_segment(sym->segdef);
                assert(sg != NULL);

                assert(sg->fragments != NULL);
                assert(sg->fragments_count <= sg->fragments_alloc);
                assert(sym->fragment < sg->fragments_count);
                frag = &sg->fragments[sym->fragment];

                fprintf(map_fp,"  %-32s %c %04lx:%08lx [0x%08lx] %20s + 0x%08lx from '%s':%u\n",
                        sym->name,
                        sym->is_local?'L':'G',
                        sg->segment_relative&0xfffful,
                        sg->segment_offset + frag->offset + sym->offset,
                        sg->linear_offset + frag->offset + sym->offset,
                        sym->segdef,
                        frag->offset + sym->offset,
                        get_in_file(sym->in_file),
                        sym->in_module);
            }
        }

        if (map_fp != NULL)
            fprintf(map_fp,"\n");
    }
}

void dump_hex_segments(FILE *hfp,const char *symbol_name) {
    static char range1[64];
    static char range2[64];
    unsigned long ressz=0;
    unsigned long firstofs=~0UL;
    unsigned int i=0,f;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        if (ressz < (sg->linear_offset+sg->segment_length))
            ressz = (sg->linear_offset+sg->segment_length);

        if (!sg->noemit) {
            fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_file_offset 0x%lxul /*file offset of base of segment*/\n",
                    symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->file_offset);

            if (firstofs == (~0UL) || firstofs > sg->file_offset)
                firstofs = sg->file_offset;
        }

        fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_resident_offset 0x%lxul /*resident offset of base of segment*/\n",
                symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->linear_offset);

        fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_segment_relative 0x%lxul /*segment value relative to resident base segment*/\n",
                symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->segment_relative);

        fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_segment_offset 0x%lxul /*offset of segment relative to segment base that contents start*/\n",
                symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->segment_offset);

        fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_segment_base 0x%lxul /*base offset added to all symbols at fixup (i.e. 0x100 for all .COM symbols)*/\n",
                symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->segment_base);

        fprintf(hfp,"#define %s_bin_segment_%s_%s_%s_length 0x%lxul\n",
                symbol_name,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",(unsigned long)sg->segment_length);

        fprintf(hfp,"/*segment=%u name='%s',class='%s',group='%s' use32=%u comb=%u big=%u fileofs=0x%lx linofs=0x%lx segbase=0x%lx segofs=0x%lx len=0x%lx segrel=0x%lx init_align=%u*/\n",
                i/*post-increment, intentional*/,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",
                sg->attr.f.f.use32,
                sg->attr.f.f.combination,
                sg->attr.f.f.big_segment,
                sg->file_offset,
                sg->linear_offset,
                sg->segment_base,
                sg->segment_offset,
                sg->segment_length,
                sg->segment_relative,
                sg->initial_alignment);

        if (sg->segment_length != 0ul) {
            sprintf(range1,"%08lx-%08lx",
                    sg->segment_offset,
                    sg->segment_offset+sg->segment_length-1ul);
            sprintf(range2,"0x%08lx-0x%08lx",
                    sg->linear_offset,
                    sg->linear_offset+sg->segment_length-1ul);
        }
        else {
            strcpy(range1,"-----------------");
            strcpy(range2,"---------------------");
        }

        fprintf(hfp,"/*  [use%02u] %-20s %-20s %-20s %04lx:%s [%s] base=0x%04lx align=%u%s%s*/\n",
                sg->attr.f.f.use32?32:16,
                sg->name?sg->name:"",
                sg->classname?sg->classname:"",
                sg->groupname?sg->groupname:"",
                sg->segment_relative&0xfffful,
                range1,
                range2,
                sg->segment_base,
                sg->initial_alignment,
                sg->pinned ? " PIN" : "",
                sg->noemit ? " NOEMIT" : "");

        if (sg->fragments != NULL) {
            for (f=0;f < sg->fragments_count;f++) {
                struct seg_fragment *frag = &sg->fragments[f];

                if (frag->fragment_length != 0ul) {
                    sprintf(range1,"%08lx-%08lx",
                            sg->segment_offset+frag->offset,
                            sg->segment_offset+frag->offset+frag->fragment_length-1ul);
                    sprintf(range2,"0x%08lx-0x%08lx",
                            sg->linear_offset+frag->offset,
                            sg->linear_offset+frag->offset+frag->fragment_length-1ul);
                }
                else {
                    strcpy(range1,"-----------------");
                    strcpy(range2,"---------------------");
                }

                fprintf(hfp,"/*  [use%02u] %-20s %-20s %-20s      %s [%s]   from '%s':%u*/\n",
                        frag->attr.f.f.use32?32:16,
                        "",
                        "",
                        "",
                        range1,
                        range2,
                        get_in_file(frag->in_file),frag->in_module);
            }
        }
    }

    fprintf(hfp,"#define %s_bin_resident_sz (%ldul)\n",symbol_name,(unsigned long)ressz);
    fprintf(hfp,"#define %s_bin_first_segment_file_offset (%ldul)\n",symbol_name,(unsigned long)firstofs);
}

void dump_link_segments(void) {
    static char range1[64];
    static char range2[64];
    unsigned int i=0,f;

    if (map_fp != NULL) {
        fprintf(map_fp,"\n");
        fprintf(map_fp,"Segment table: %u entries\n",(unsigned int)link_segments_count);
        fprintf(map_fp,"---------------------------------------\n");
    }

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        if (cmdoptions.verbose) {
            fprintf(stderr,"segment[%u]: name='%s' class='%s' group='%s' use32=%u comb=%u big=%u fileofs=0x%lx linofs=0x%lx segbase=0x%lx segofs=0x%lx len=0x%lx segrel=0x%lx init_align=%u\n",
                    i/*post-increment, intentional*/,sg->name?sg->name:"",sg->classname?sg->classname:"",sg->groupname?sg->groupname:"",
                    sg->attr.f.f.use32,
                    sg->attr.f.f.combination,
                    sg->attr.f.f.big_segment,
                    sg->file_offset,
                    sg->linear_offset,
                    sg->segment_base,
                    sg->segment_offset,
                    sg->segment_length,
                    sg->segment_relative,
                    sg->initial_alignment);
        }

        if (map_fp != NULL) {
            if (sg->segment_length != 0ul) {
                sprintf(range1,"%08lx-%08lx",
                    sg->segment_offset,
                    sg->segment_offset+sg->segment_length-1ul);
                sprintf(range2,"0x%08lx-0x%08lx",
                    sg->linear_offset,
                    sg->linear_offset+sg->segment_length-1ul);
            }
            else {
                strcpy(range1,"-----------------");
                strcpy(range2,"---------------------");
            }

            fprintf(map_fp,"  [use%02u] %-20s %-20s %-20s %04lx:%s [%s] base=0x%04lx align=%u%s%s\n",
                sg->attr.f.f.use32?32:16,
                sg->name?sg->name:"",
                sg->classname?sg->classname:"",
                sg->groupname?sg->groupname:"",
                sg->segment_relative&0xfffful,
                range1,
                range2,
                sg->segment_base,
                sg->initial_alignment,
                sg->pinned ? " PIN" : "",
                sg->noemit ? " NOEMIT" : "");
        }

        if (sg->fragments != NULL) {
            for (f=0;f < sg->fragments_count;f++) {
                struct seg_fragment *frag = &sg->fragments[f];

                if (cmdoptions.verbose) {
                    fprintf(stderr,"  fragment[%u]: file='%s' module=%u offset=0x%lx length=0x%lx segidx=%u\n",
                            f,get_in_file(frag->in_file),frag->in_module,frag->offset,frag->fragment_length,frag->segidx);
                }

                if (map_fp != NULL) {
                    if (frag->fragment_length != 0ul) {
                        sprintf(range1,"%08lx-%08lx",
                            sg->segment_offset+frag->offset,
                            sg->segment_offset+frag->offset+frag->fragment_length-1ul);
                        sprintf(range2,"0x%08lx-0x%08lx",
                            sg->linear_offset+frag->offset,
                            sg->linear_offset+frag->offset+frag->fragment_length-1ul);
                    }
                    else {
                        strcpy(range1,"-----------------");
                        strcpy(range2,"---------------------");
                    }

                    fprintf(map_fp,"  [use%02u] %-20s %-20s %-20s      %s [%s]   from '%s':%u\n",
                            frag->attr.f.f.use32?32:16,
                            "",
                            "",
                            "",
                            range1,
                            range2,
                            get_in_file(frag->in_file),frag->in_module);
                }
            }
        }
    }

    if (map_fp != NULL)
        fprintf(map_fp,"\n");
}

struct link_segdef *find_link_segment_by_grpdef(const char *name) {
    unsigned int i=0;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        if (sg->groupname == NULL) continue;
        if (!strcmp(name,sg->groupname)) return sg;
    }

    return NULL;
}

struct link_segdef *find_link_segment_by_class(const char *name) {
    unsigned int i=0;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        if (sg->classname == NULL) continue;
        if (!strcmp(name,sg->classname)) return sg;
    }

    return NULL;
}

struct link_segdef *find_link_segment_by_class_last(const char *name) {
    struct link_segdef *ret=NULL;
    unsigned int i=0;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        if (sg->classname == NULL) continue;
        if (!strcmp(name,sg->classname)) ret = sg;
    }

    return ret;
}

struct link_segdef *find_link_segment(const char *name) {
    unsigned int i=0;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

        assert(sg->name != NULL);
        if (!strcmp(name,sg->name)) return sg;
    }

    return NULL;
}

struct link_segdef *new_link_segment(const char *name) {
    if (link_segments_count < MAX_SEGMENTS) {
        struct link_segdef *sg = &link_segments[link_segments_count++];

        memset(sg,0,sizeof(*sg));
        sg->name = strdup(name);
        assert(sg->name != NULL);

        return sg;
    }

    return NULL;
}

int ledata_add(struct omf_context_t *omf_state, struct omf_ledata_info_t *info,unsigned int pass) {
    struct seg_fragment *frag;
    struct link_segdef *lsg;
    unsigned long max_ofs;
    const char *segname;

    segname = omf_context_get_segdef_name_safe(omf_state, info->segment_index);
    if (*segname == 0) {
        fprintf(stderr,"Null segment name\n");
        return 1;
    }

    if ((lsg=find_link_segment(segname)) == NULL) {
        fprintf(stderr,"Segment %s not found\n",segname);
        return 1;
    }

    if (lsg->noemit) return 0;

    current_link_segment = lsg;

    if (info->data_length == 0)
        return 0;

    if (lsg->fragments == NULL || lsg->fragments_count == 0) {
        fprintf(stderr,"LEDATA when no fragments defined (bug?)\n");
        return 1;
    }

    assert(lsg->fragments_read > 0 && lsg->fragments_read <= lsg->fragments_count);
    frag = &lsg->fragments[lsg->fragments_read-1];

    max_ofs = (unsigned long)info->enum_data_offset + (unsigned long)info->data_length + (unsigned long)frag->offset;
    if (lsg->segment_length < max_ofs) {
        fprintf(stderr,"LEDATA out of bounds (len=%lu max=%lu)\n",lsg->segment_length,max_ofs);
        return 1;
    }

    if (pass == PASS_BUILD) {
        assert(info->data != NULL);
        assert(max_ofs >= (unsigned long)info->data_length);
        max_ofs -= (unsigned long)info->data_length;
        assert(lsg->image_ptr != NULL);
        memcpy(lsg->image_ptr + max_ofs, info->data, info->data_length);
    }

    if (cmdoptions.verbose)
        fprintf(stderr,"LEDATA '%s' base=0x%lx offset=0x%lx len=%lu enumo=0x%lx in frag ofs=0x%lx\n",
                segname,lsg->load_base,max_ofs,info->data_length,info->enum_data_offset,frag->offset);

    return 0;
}

int fixupp_get(struct omf_context_t *omf_state,unsigned long *fseg,unsigned long *fofs,struct link_segdef **sdef,const struct omf_fixupp_t *ent,unsigned int method,unsigned int index,unsigned int in_file,unsigned int in_module) {
    *fseg = *fofs = ~0UL;
    *sdef = NULL;
    (void)ent;

    if (method == 0/*SEGDEF*/) {
        struct link_segdef *lsg;
        const char *segname;

        segname = omf_context_get_segdef_name_safe(omf_state,index);
        if (*segname == 0) {
            fprintf(stderr,"FIXUPP SEGDEF no name\n");
            return -1;
        }

        lsg = find_link_segment(segname);
        if (lsg == NULL) {
            fprintf(stderr,"FIXUPP SEGDEF not found '%s'\n",segname);
            return -1;
        }

        *fseg = lsg->segment_relative;
        *fofs = lsg->segment_offset;
        *sdef = lsg;
    }
    else if (method == 1/*GRPDEF*/) {
        struct link_segdef *lsg;
        const char *segname;

        segname = omf_context_get_grpdef_name_safe(omf_state,index);
        if (*segname == 0) {
            fprintf(stderr,"FIXUPP SEGDEF no name\n");
            return -1;
        }

        lsg = find_link_segment_by_grpdef(segname);
        if (lsg == NULL) {
            fprintf(stderr,"FIXUPP SEGDEF not found\n");
            return -1;
        }

        *fseg = lsg->segment_relative;
        *fofs = lsg->segment_offset;
        *sdef = lsg;
    }
    else if (method == 2/*EXTDEF*/) {
        struct seg_fragment *frag;
        struct link_segdef *lsg;
        struct link_symbol *sym;
        const char *defname;

        defname = omf_context_get_extdef_name_safe(omf_state,index);
        if (*defname == 0) {
            fprintf(stderr,"FIXUPP EXTDEF no name\n");
            return -1;
        }

        sym = find_link_symbol(defname,in_file,in_module);
        if (sym == NULL) {
            fprintf(stderr,"No such symbol '%s'\n",defname);
            return -1;
        }

        assert(sym->segdef != NULL);
        lsg = find_link_segment(sym->segdef);
        if (lsg == NULL) {
            fprintf(stderr,"FIXUPP SEGDEF for EXTDEF not found '%s'\n",sym->segdef);
            return -1;
        }

        assert(lsg->fragments != NULL);
        assert(lsg->fragments_count <= lsg->fragments_alloc);
        assert(sym->fragment < lsg->fragments_count);
        frag = &lsg->fragments[sym->fragment];

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

int apply_FIXUPP(struct omf_context_t *omf_state,unsigned int first,unsigned int in_file,unsigned int in_module,unsigned int pass) {
    unsigned long final_seg,final_ofs;
    unsigned long frame_seg,frame_ofs;
    unsigned long targ_seg,targ_ofs;
    struct link_segdef *frame_sdef;
    struct link_segdef *targ_sdef;
    const struct omf_segdef_t *cur_segdef;
    struct seg_fragment *frag;
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

        current_link_segment = find_link_segment(cur_segdefname);
        if (current_link_segment == NULL) {
            fprintf(stderr,"Cannot find linker segment '%s'\n",cur_segdefname);
            return 1;
        }

        /* assuming each OBJ/module has only one of each named segment,
         * get the fragment it belongs to */
        assert(current_link_segment->fragments_read > 0);
        assert(current_link_segment->fragments_read <= current_link_segment->fragments_count);
        frag = &current_link_segment->fragments[current_link_segment->fragments_read-1];

        assert(frag->in_file == in_file);
        assert(frag->in_module == in_module);

        if (pass == PASS_BUILD) {
            assert(current_link_segment != NULL);
            assert(current_link_segment->image_ptr != NULL);
            fence = current_link_segment->image_ptr + current_link_segment->segment_length;

            ptch =  (unsigned long)ent->omf_rec_file_enoffs +
                (unsigned long)ent->data_record_offset +
                (unsigned long)frag->offset;

            if (omf_state->flags.verbose)
                fprintf(stderr,"ptch=0x%lx linear=0x%lx load=0x%lx '%s'\n",
                        ptch,
                        current_link_segment->linear_offset,
                        ent->omf_rec_file_enoffs + ent->data_record_offset,
                        current_link_segment->name);

            ptr = current_link_segment->image_ptr + ptch;
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
                            dump_link_segments();
                            fprintf(stderr,"FIXUPP: self-relative offset fixup across segments with different bases not allowed\n");
                            fprintf(stderr,"        FIXUP in segment '%s' base 0x%lx\n",
                                current_link_segment->name,
                                current_link_segment->segment_relative);
                            fprintf(stderr,"        FIXUP to segment '%s' base 0x%lx\n",
                                targ_sdef->name,
                                targ_sdef->segment_relative);
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

                if (output_format == OFMT_COM || output_format == OFMT_DOSDRV) {
                    if (output_format_variant == OFMTVAR_COMREL) {
                        if (pass == PASS_GATHER) {
                            struct exe_relocation *reloc = new_exe_relocation();
                            if (reloc == NULL) {
                                fprintf(stderr,"Unable to allocate relocation\n");
                                return -1;
                            }

                            assert(current_link_segment->fragments_read > 0);
                            assert(current_link_segment->name != NULL);
                            reloc->segname = current_link_segment->name;
                            reloc->fragment = current_link_segment->fragments_read - 1u;
                            reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset;

                            if (cmdoptions.verbose)
                                fprintf(stderr,"COM relocation entry: Patch up %s:%u:%04lx\n",reloc->segname.c_str(),reloc->fragment,reloc->offset);
                        }

                        if (pass == PASS_BUILD) {
                            *((uint16_t*)ptr) += (uint16_t)targ_sdef->segment_relative;
                        }
                    }
                    else {
                        fprintf(stderr,"segment base self-relative not supported for .COM\n");
                        return -1;
                    }
                }
                else if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                    /* emit as a relocation */
                    if (pass == PASS_GATHER) {
                        struct exe_relocation *reloc = new_exe_relocation();
                        if (reloc == NULL) {
                            fprintf(stderr,"Unable to allocate relocation\n");
                            return -1;
                        }

                        assert(current_link_segment->fragments_read > 0);
                        assert(current_link_segment->name != NULL);
                        reloc->segname = current_link_segment->name;
                        reloc->fragment = current_link_segment->fragments_read - 1u;
                        reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset;

                        if (cmdoptions.verbose)
                            fprintf(stderr,"EXE relocation entry: Patch up %s:%u:%04lx\n",reloc->segname.c_str(),reloc->fragment,reloc->offset);
                    }

                    if (pass == PASS_BUILD) {
                        *((uint16_t*)ptr) += (uint16_t)targ_sdef->segment_relative;
                    }
                }
                else {
                    if (pass == PASS_BUILD) {
                        *((uint16_t*)ptr) += (uint16_t)targ_sdef->segment_relative;
                    }
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

                if (output_format == OFMT_COM || output_format == OFMT_DOSDRV) {
                    if (output_format_variant == OFMTVAR_COMREL) {
                        if (pass == PASS_GATHER) {
                            struct exe_relocation *reloc = new_exe_relocation();
                            if (reloc == NULL) {
                                fprintf(stderr,"Unable to allocate relocation\n");
                                return -1;
                            }

                            assert(current_link_segment->fragments_read > 0);
                            assert(current_link_segment->name != NULL);
                            reloc->segname = current_link_segment->name;
                            reloc->fragment = current_link_segment->fragments_read - 1u;
                            reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset + 2u;

                            if (cmdoptions.verbose)
                                fprintf(stderr,"COM relocation entry: Patch up %s:%u:%04lx\n",reloc->segname.c_str(),reloc->fragment,reloc->offset);
                        }

                        if (pass == PASS_BUILD) {
                            *((uint16_t*)ptr) += (uint16_t)final_ofs;
                            *((uint16_t*)(ptr+2)) += (uint16_t)targ_sdef->segment_relative;
                        }
                    }
                    else {
                        fprintf(stderr,"segment base self-relative not supported for .COM\n");
                        return -1;
                    }
                }
                else if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                    /* emit as a relocation */
                    if (pass == PASS_GATHER) {
                        struct exe_relocation *reloc = new_exe_relocation();
                        if (reloc == NULL) {
                            fprintf(stderr,"Unable to allocate relocation\n");
                            return -1;
                        }

                        assert(current_link_segment->fragments_read > 0);
                        assert(current_link_segment->name != NULL);
                        reloc->segname = current_link_segment->name;
                        reloc->fragment = current_link_segment->fragments_read - 1u;
                        reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset + 2u;

                        if (cmdoptions.verbose)
                            fprintf(stderr,"EXE relocation entry: Patch up %s:%u:%04lx\n",reloc->segname.c_str(),reloc->fragment,reloc->offset);
                    }

                    if (pass == PASS_BUILD) {
                        *((uint16_t*)ptr) += (uint16_t)final_ofs;
                        *((uint16_t*)(ptr+2)) += (uint16_t)targ_sdef->segment_relative;
                    }
                }
                else {
                    if (pass == PASS_BUILD) {
                        *((uint16_t*)ptr) += (uint16_t)final_ofs;
                        *((uint16_t*)(ptr+2)) += (uint16_t)targ_sdef->segment_relative;
                    }
                }
                break;
            case OMF_FIXUPP_LOCATION_32BIT_OFFSET: /* 32-bit offset */
                if (pass == PASS_BUILD) {
                    assert((ptr+4) <= fence);

                    if (!ent->segment_relative) {
                        /* sanity check: self-relative is only allowed IF the same segment */
                        /* we could fidget about with relative fixups across real-mode segments, but I'm not going to waste my time on that */
                        if (current_link_segment->segment_relative != targ_sdef->segment_relative) {
                            dump_link_segments();
                            fprintf(stderr,"FIXUPP: self-relative offset fixup across segments with different bases not allowed\n");
                            fprintf(stderr,"        FIXUP in segment '%s' base 0x%lx\n",
                                current_link_segment->name,
                                current_link_segment->segment_relative);
                            fprintf(stderr,"        FIXUP to segment '%s' base 0x%lx\n",
                                targ_sdef->name,
                                targ_sdef->segment_relative);
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
        struct link_segdef *lsg;
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

                lsg = find_link_segment(segdef_name);
                if (lsg == NULL) {
                    fprintf(stderr,"GRPDEF refers to SEGDEF that has not been registered\n");
                    return 1;
                }

                if (lsg->groupname == NULL) {
                    /* assign to group */
                    lsg->groupname = strdup(grpdef_name);
                }
                else if (!strcmp(lsg->groupname, grpdef_name)) {
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

int pubdef_add(struct omf_context_t *omf_state,unsigned int first,unsigned int tag,unsigned int in_file,unsigned int in_module,unsigned int pass) {
    const unsigned char is_local = (tag == OMF_RECTYPE_LPUBDEF) || (tag == OMF_RECTYPE_LPUBDEF32);

    (void)pass;

    while (first < omf_state->PUBDEFs.omf_PUBDEFS_count) {
        const struct omf_pubdef_t *pubdef = &omf_state->PUBDEFs.omf_PUBDEFS[first++];
        struct link_segdef *lsg;
        struct link_symbol *sym;
        const char *groupname;
        const char *segname;
        const char *name;

        if (pubdef == NULL) continue;
        name = pubdef->name_string;
        if (name == NULL) continue;
        segname = omf_context_get_segdef_name_safe(omf_state,pubdef->segment_index);
        if (*segname == 0) continue;
        groupname = omf_context_get_grpdef_name_safe(omf_state,pubdef->group_index);

        lsg = find_link_segment(segname);
        if (lsg == NULL) {
            fprintf(stderr,"Pubdef: No such segment '%s'\n",segname);
            return -1;
        }

        if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
        }
        else {
            /* no symbols allowed in STACK.
             * BSS is allowed. */
            if (!strcasecmp(segname,"_STACK") || !strcasecmp(segname,"STACK") ||
                (lsg->classname != NULL && (!strcasecmp(lsg->classname,"STACK")))) {
                fprintf(stderr,"Emitting symbols to STACK segment not permitted for COM/DRV output\n");
                return 1;
            }
        }

        if (cmdoptions.verbose)
            fprintf(stderr,"pubdef[%u]: '%s' group='%s' seg='%s' offset=0x%lx finalofs=0x%lx local=%u\n",
                    first,name,groupname,segname,(unsigned long)pubdef->public_offset,pubdef->public_offset + lsg->load_base,is_local);

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
        assert(sym->groupdef == NULL);
        assert(sym->segdef == NULL);

        assert(pass == PASS_GATHER);
        assert(lsg->fragments != NULL);
        assert(lsg->fragments_count != 0);

        sym->fragment = lsg->fragments_count - 1u;
        sym->offset = pubdef->public_offset;
        sym->groupdef = strdup(groupname);
        sym->segdef = strdup(segname);
        sym->in_file = in_file;
        sym->in_module = in_module;
        sym->is_local = is_local;
    }

    return 0;
}

int segdef_add(struct omf_context_t *omf_state,unsigned int first,unsigned int in_file,unsigned int in_module,unsigned int pass) {
    unsigned long alignb,malign;
    struct link_segdef *lsg;

    while (first < omf_state->SEGDEFs.omf_SEGDEFS_count) {
        struct omf_segdef_t *sg = &omf_state->SEGDEFs.omf_SEGDEFS[first++];
        const char *classname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->class_name_index);
        const char *name = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->segment_name_index);

        if (*name == 0) continue;

        if (pass == PASS_BUILD) {
            lsg = find_link_segment(name);
            if (lsg == NULL) {
                fprintf(stderr,"No SEGDEF '%s'\n",name);
                return -1;
            }
            if (lsg->fragments == NULL)
                continue;
            if (lsg->fragments_count == 0)
                continue;

            assert(lsg->fragments_read < lsg->fragments_count);

            {
                struct seg_fragment *f = &lsg->fragments[lsg->fragments_read++];

                assert(f->in_file == in_file);
                assert(f->in_module == in_module);
                assert(f->fragment_length == sg->segment_length);
                assert(f->segidx == first);

                lsg->load_base = f->offset;
            }
        }
        else if (pass == PASS_GATHER) {
            lsg = find_link_segment(name);
            if (lsg != NULL) {
                /* it is an error to change attributes */
                if (cmdoptions.verbose)
                    fprintf(stderr,"SEGDEF class='%s' name='%s' already exits\n",classname,name);

                lsg->attr.f.f.alignment = sg->attr.f.f.alignment;
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

                assert(lsg->classname == NULL);
                lsg->classname = strdup(classname);

                lsg->attr = sg->attr;
                lsg->initial_alignment = omf_align_code_to_bytes(lsg->attr.f.f.alignment);
            }

            /* alignment */
            alignb = omf_align_code_to_bytes(lsg->attr.f.f.alignment);
            malign = lsg->segment_len_count % (unsigned long)alignb;
            if (malign != 0) lsg->segment_len_count += alignb - malign;
            lsg->load_base = lsg->segment_len_count;
            lsg->segment_len_count += sg->segment_length;
            lsg->segment_length = lsg->segment_len_count;

            {
                struct seg_fragment *f = alloc_link_segment_fragment(lsg);
                if (f == NULL) {
                    fprintf(stderr,"Unable to alloc segment fragment\n");
                    return -1;
                }

                f->in_file = in_file;
                f->in_module = in_module;
                f->offset = lsg->load_base;
                f->fragment_length = sg->segment_length;
                f->segidx = first;
                f->attr = sg->attr;
            }

            if (cmdoptions.verbose)
                fprintf(stderr,"Start segment='%s' load=0x%lx\n",
                        lsg->name, lsg->load_base);
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

int segment_def_arrange(void) {
    unsigned long m,ofs = 0;
    unsigned int inf;

    for (inf=0;inf < link_segments_count;inf++) {
        struct link_segdef *sd = &link_segments[inf];

        if (sd->initial_alignment != 0ul) {
            m = ofs % sd->initial_alignment;
            if (m != 0ul) ofs += sd->initial_alignment - m;
        }

        if (cmdoptions.verbose)
            fprintf(stderr,"segment[%u] ofs=0x%lx len=0x%lx\n",
                    inf,ofs,sd->segment_length);

        if (output_format == OFMT_COM || output_format == OFMT_EXE ||
            output_format == OFMT_DOSDRV || output_format == OFMT_DOSDRVEXE) {
            if (sd->segment_length > 0x10000ul) {
                dump_link_segments();
                fprintf(stderr,"Segment too large >= 64KB\n");
                return -1;
            }
        }

        sd->linear_offset = ofs;
        ofs += sd->segment_length;
    }

    return 0;
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
                cmdoptions.in_file.push_back(s); /* constructs std::string from char* */
            }
            else if (!strcmp(a,"hsym")) {
                a = argv[i++];
                if (a == NULL) return 1;
                cmdoptions.dosdrv_header_symbol = a;
            }
            else if (!strcmp(a,"pflat")) {
                prefer_flat = 1;
            }
            else if (!strncmp(a,"stack",5)) {
                a += 5;
                if (!isxdigit(*a)) return 1;
                cmdoptions.want_stack_size = strtoul(a,NULL,16);
            }
            else if (!strncmp(a,"com",3)) {
                a += 3;
                if (!isxdigit(*a)) return 1;
                cmdoptions.com_segbase = strtoul(a,NULL,16);
            }
            else if (!strcmp(a,"hex")) {
                a = argv[i++];
                if (a == NULL) return 1;
                cmdoptions.hex_output = a;
            }
            else if (!strcmp(a,"hexcpp")) {
                cmdoptions.hex_cpp = true;
            }
            else if (!strcmp(a,"hexsplit")) {
                cmdoptions.hex_split = true;
            }
            else if (!strcmp(a,"of")) {
                a = argv[i++];
                if (a == NULL) return 1;

                if (!strcmp(a,"com"))
                    output_format = OFMT_COM;
                else if (!strcmp(a,"comrel")) {
                    output_format = OFMT_COM;
                    output_format_variant = OFMTVAR_COMREL;
                }
                else if (!strcmp(a,"exe"))
                    output_format = OFMT_EXE;
                else if (!strcmp(a,"dosdrv"))
                    output_format = OFMT_DOSDRV;
                else if (!strcmp(a,"dosdrvrel")) {
                    output_format = OFMT_DOSDRV;
                    output_format_variant = OFMTVAR_COMREL;
                }
                else if (!strcmp(a,"dosdrvexe"))
                    output_format = OFMT_DOSDRVEXE;
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

    if (cmdoptions.com_segbase == segmentBaseUndef) {
        if (output_format == OFMT_COM) {
            cmdoptions.com_segbase = 0x100;
        }
        else if (output_format == OFMT_EXE || output_format == OFMT_DOSDRV || output_format == OFMT_DOSDRVEXE) {
            cmdoptions.com_segbase = 0;
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

    if (output_format == OFMT_COM) {
        struct link_segdef *sg;

        sg = find_link_segment("__COM_ENTRY_JMP");
        if (sg != NULL) return 1;

        sg = new_link_segment("__COM_ENTRY_JMP");
        if (sg == NULL) return 1;

        sg->classname = strdup("CODE");
        sg->initial_alignment = 1;
        sg->pinned = 1;
    }

    for (pass=0;pass < PASS_MAX;pass++) {
        for (current_in_file=0;current_in_file < cmdoptions.in_file.size();current_in_file++) {
            assert(!cmdoptions.in_file[current_in_file].empty());

            fd = open(cmdoptions.in_file[current_in_file].c_str(),O_RDONLY|O_BINARY);
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

            do {
                ret = omf_context_read_fd(omf_state,fd);
                if (ret == 0) {
                    if (apply_FIXUPP(omf_state,0,current_in_file,current_in_file_module,pass))
                        return 1;
                    omf_fixupps_context_free_entries(&omf_state->FIXUPPs);

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

                            if (pass == PASS_BUILD && ledata_add(omf_state, &info, pass))
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
                                        struct link_segdef *frameseg,*targseg;

                                        targseg = find_link_segment(targetname);
                                        frameseg = find_link_segment(framename);
                                        if (targseg != NULL && frameseg != NULL) {
                                            entry_seg_ofs = TargetDisplacement;

                                            assert(frameseg->fragments_count != 0);
                                            entry_seg_link_frame_fragment = frameseg->fragments_count - 1u;

                                            assert(targseg->fragments_count != 0);
                                            entry_seg_link_target_fragment = targseg->fragments_count - 1u;

                                            entry_seg_link_target_name = strdup(targetname);
                                            entry_seg_link_target = targseg;
                                            entry_seg_link_frame_name = strdup(framename);
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

            close(fd);
        }

        if (pass == PASS_GATHER) {
            unsigned long file_baseofs = 0;

            if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                struct link_segdef *stacksg = find_link_segment_by_class_last("STACK");

                if (stacksg != NULL) {
                    assert(stacksg->image_ptr == NULL);

                    if (stacksg->segment_length < cmdoptions.want_stack_size) {
                        struct seg_fragment *frag;

                        frag = alloc_link_segment_fragment(stacksg);
                        if (frag == NULL) {
                            return 1;
                        }
                        frag->offset = stacksg->segment_length;
                        frag->segidx = (int)(stacksg + 1 - (&link_segments[0]));
                        frag->attr = stacksg->attr;
                        frag->fragment_length = cmdoptions.want_stack_size - stacksg->segment_length;
                        frag->in_file = in_fileRefInternal;

                        stacksg->segment_length = cmdoptions.want_stack_size;
                    }
                }
            }

            owlink_default_sort_seg();

            if (cmdoptions.do_dosseg)
                owlink_dosseg_sort_order();

            {
                struct link_segdef *ssg;
                unsigned int i;

                for (i=0;i < link_segments_count;i++) {
                    ssg = &link_segments[i];
                    if (ssg->classname == NULL) continue;

                    if (!strcmp(ssg->classname,"STACK") ||
                        !strcmp(ssg->classname,"BSS")) {
                        ssg->noemit = 1;
                    }
                }
            }

            /* entry point checkup */
            if (output_format == OFMT_DOSDRV) {
                /* MS-DOS device drivers do NOT have an entry point */
                if (entry_seg_link_target != NULL) {
                    fprintf(stderr,"WARNING: MS-DOS device drivers, flat format (.SYS) should not have entry point.\n");
                    fprintf(stderr,"         Entry point provided by input OBJs will be ignored.\n");
                }
            }
            else {
                if (entry_seg_link_target == NULL) {
                    fprintf(stderr,"WARNING: No entry point found\n");
                }
            }

            /* entry point cannot be 32-bit */
            if (output_format == OFMT_DOSDRV) {
                /* nothing */
            }
            else if (entry_seg_link_target != NULL) {
                struct seg_fragment *frag;

                assert(entry_seg_link_target->fragments != NULL);
                assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);
                frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

                if (frag->attr.f.f.use32) {
                    fprintf(stderr,"Entry point cannot be 32-bit\n");
                    return 1;
                }
            }

            /* put segments in order, linear offset */
            {
                /* COMREL relocation + patch code */
                if ((output_format == OFMT_COM || output_format == OFMT_DOSDRV) &&
                    output_format_variant == OFMTVAR_COMREL && !exe_relocation_table.empty()) {
                    /* make a new segment attached to the end, containing the relocation
                     * table and the patch up code, which becomes the new entry point. */
                    struct seg_fragment *tfrag;
                    struct seg_fragment *frag;
                    struct link_segdef *tsg;
                    struct link_segdef *sg;

                    sg = find_link_segment("__COMREL_RELOC");
                    if (sg == NULL) {
                        sg = new_link_segment("__COMREL_RELOC");
                        if (sg == NULL) {
                            fprintf(stderr,"Cannot allocate COMREL relocation segment\n");
                            return 1;
                        }

                        sg->initial_alignment = 1;
                        sg->classname = strdup("CODE");
                    }

                    /* __COMREL_RELOC cannot be a 32-bit segment */
                    if (sg->attr.f.f.use32) {
                        fprintf(stderr,"__COMREL_RELOC cannot be a 32-bit segment\n");
                        return 1;
                    }

                    tsg = find_link_segment("__COMREL_RELOCTBL");

                    frag = alloc_link_segment_fragment(sg);
                    if (frag == NULL) {
                        return 1;
                    }
                    frag->offset = sg->segment_length;
                    frag->segidx = (int)(sg + 1 - (&link_segments[0]));
                    frag->in_file = in_fileRefInternal;
                    frag->attr = sg->attr;

                    if (tsg != NULL) {
                        tfrag = alloc_link_segment_fragment(tsg);
                        if (tfrag == NULL) {
                            return 1;
                        }
                        tfrag->offset = tsg->segment_length;
                        tfrag->segidx = (int)(tsg + 1 - (&link_segments[0]));
                        tfrag->in_file = in_fileRefInternal;
                        tfrag->attr = tsg->attr;
                    }
                    else {
                        tsg = sg;
                        tfrag = frag;
                    }

                    tsg->segment_length += exe_relocation_table.size() * (size_t)2;

                    if (output_format == OFMT_DOSDRV)
                        sg->segment_length += sizeof(dosdrvrel_entry_point);
                    else
                        sg->segment_length += sizeof(comrel_entry_point);

                    frag->fragment_length = sg->segment_length - frag->offset;
                    if (tfrag != frag)
                        tfrag->fragment_length = tsg->segment_length - tfrag->offset;
                }

                /* .COM format: if the entry point is nonzero, a JMP instruction
                 * must be inserted at the start to JMP to the entry point */
                if (output_format == OFMT_DOSDRV) {
                }
                else if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                    /* EXE */
                    /* TODO: relocation table */
                    file_baseofs = 32;
                }

                owlink_stack_bss_arrange();

                if (segment_def_arrange())
                    return 1;

                if (output_format == OFMT_COM) {
                    struct link_segdef *sg;

                    sg = find_link_segment("__COM_ENTRY_JMP");
                    assert(sg != NULL);
                    assert(sg->segment_length == 0);

                    /* COM relocatable */
                    if (output_format_variant == OFMTVAR_COMREL) {
                        /* always make room */
                        com_entry_insert = 3;
                        sg->segment_length = com_entry_insert;

                        if (cmdoptions.verbose)
                            fprintf(stderr,"Entry point needed for relocateable .COM\n");
                    }
                    /* COM */
                    else if (entry_seg_link_target != NULL) {
                        struct seg_fragment *frag;
                        unsigned long io;

                        assert(entry_seg_link_target->fragments != NULL);
                        assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);
                        frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

                        io = (entry_seg_link_target->linear_offset+entry_seg_ofs+frag->offset);

                        if (io != 0) {
                            fprintf(stderr,"Entry point is not start of executable, required by .COM format.\n");
                            fprintf(stderr,"Adding JMP instruction to compensate.\n");

                            if (io >= 0x82) /* too far for 2-byte JMP */
                                com_entry_insert = 3;
                            else
                                com_entry_insert = 2;

                            sg->segment_length = com_entry_insert;
                        }
                    }

                    {
                        struct seg_fragment *frag = alloc_link_segment_fragment(sg);
                        if (frag == NULL) return 1;

                        frag->offset = 0;
                        frag->fragment_length = sg->segment_length;
                        frag->in_file = in_fileRefInternal;
                        frag->attr = sg->attr;

                        if (sg->segment_length > 0) {
                            struct link_symbol *sym;

                            sym = new_link_symbol("__COM_ENTRY_JMP_INS");
                            if (sym == NULL) return 1;
                            sym->offset = 0;
                            sym->in_file = in_fileRefInternal;
                            sym->fragment = (int)(frag - &sg->fragments[0]);
                            sym->segdef = strdup("__COM_ENTRY_JMP");
                        }
                    }

                    if (segment_def_arrange())
                        return 1;
                }
            }

            /* if a .COM executable, then all segments are arranged so that the first byte
             * is at 0x100 */
            if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                unsigned long segrel = 0;
                unsigned int linkseg;

                for (linkseg=0;linkseg < link_segments_count;linkseg++) {
                    struct link_segdef *sd = &link_segments[linkseg];
                    struct link_segdef *gd = sd->groupname != NULL ? find_link_segment_by_grpdef(sd->groupname) : 0;
                    struct link_segdef *cd = sd->classname != NULL ? find_link_segment_by_class(sd->classname) : 0;

                    if (gd != NULL)
                        segrel = gd->linear_offset >> 4ul;
                    else if (cd != NULL)
                        segrel = cd->linear_offset >> 4ul;
                    else
                        segrel = sd->linear_offset >> 4ul;

                    if (prefer_flat && sd->linear_offset < (0xFFFFul - cmdoptions.com_segbase))
                        segrel = 0; /* user prefers flat .COM memory model, where possible */

                    sd->segment_base = cmdoptions.com_segbase;
                    sd->segment_relative = segrel - (cmdoptions.com_segbase >> 4ul);
                    sd->segment_offset = cmdoptions.com_segbase + sd->linear_offset - (segrel << 4ul);

                    if (sd->segment_offset >= 0xFFFFul) {
                        dump_link_segments();
                        fprintf(stderr,"EXE: segment offset out of range\n");
                        return -1;
                    }
                }
            }
            else if (output_format == OFMT_COM || output_format == OFMT_DOSDRV) {
                unsigned int linkseg;

                for (linkseg=0;linkseg < link_segments_count;linkseg++) {
                    struct link_segdef *sd = &link_segments[linkseg];

                    sd->segment_relative = 0;
                    sd->segment_base = cmdoptions.com_segbase;
                    sd->segment_offset = cmdoptions.com_segbase + sd->linear_offset;

                    if (sd->segment_offset >= 0xFFFFul) {
                        dump_link_segments();
                        fprintf(stderr,"COM: segment offset out of range\n");
                        return -1;
                    }
                }
            }
            else {
                abort();
            }

            /* decide where the segments end up in the executable */
            {
                unsigned long ofs = 0;
                unsigned int linkseg;

                if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
                    /* TODO: EXE header */
                }

                for (linkseg=link_segments_count;linkseg > 0;) {
                    struct link_segdef *sd = &link_segments[--linkseg];

                    if (!sd->noemit) break;
                }

                for (;linkseg > 0;) {
                    struct link_segdef *sd = &link_segments[--linkseg];

                    if (sd->noemit) {
                        fprintf(stderr,"Warning, segment '%s' marked NOEMIT will be emitted due to COM/EXE format constraints.\n",
                            sd->name);

                        if (map_fp != NULL)
                            fprintf(map_fp,"* Warning, segment '%s' marked NOEMIT will be emitted due to COM/EXE format constraints.\n",
                                sd->name);

                        sd->noemit = 0;
                    }
                }

                for (linkseg=0;linkseg < link_segments_count;linkseg++) {
                    struct link_segdef *sd = &link_segments[linkseg];

                    if (sd->noemit) break;

                    ofs = sd->linear_offset + file_baseofs;

                    sd->file_offset = ofs;

                    /* NTS: for EXE files, sd->file_offset will be adjusted further downward for relocation tables.
                     *      in fact, maybe computing file offset at this phase was a bad idea... :( */
                }

                for (;linkseg < link_segments_count;linkseg++) {
                    struct link_segdef *sd = &link_segments[linkseg];

                    assert(sd->noemit != 0);
                }
            }

            /* allocate in-memory copy of the segments */
            {
                unsigned int linkseg;

                for (linkseg=0;linkseg < link_segments_count;linkseg++) {
                    struct link_segdef *sd = &link_segments[linkseg];

                    assert(sd->image_ptr == NULL);
                    if (sd->segment_length != 0 && !sd->noemit) {
                        sd->image_ptr = (unsigned char*)malloc(sd->segment_length);
                        assert(sd->image_ptr != NULL);
                        memset(sd->image_ptr,0,sd->segment_length);
                    }

                    /* reset load base */
                    sd->segment_len_count = 0;
                    sd->fragments_read = 0;
                    sd->load_base = 0;
                }
            }

            /* COMREL relocation + patch code */
            if ((output_format == OFMT_COM || output_format == OFMT_DOSDRV) &&
                output_format_variant == OFMTVAR_COMREL && !exe_relocation_table.empty()) {
                /* make a new segment attached to the end, containing the relocation
                 * table and the patch up code, which becomes the new entry point. */
                struct link_segdef *sg;
                struct link_symbol *sym;
                struct seg_fragment *frag;

                struct link_segdef *tsg;
                struct seg_fragment *tfrag;

                sg = find_link_segment("__COMREL_RELOC");
                if (sg == NULL) {
                    fprintf(stderr,"COMREL relocation segment missing\n");
                    return 1;
                }

                /* __COMREL_RELOC cannot be a 32-bit segment */
                if (sg->attr.f.f.use32) {
                    fprintf(stderr,"__COMREL_RELOC cannot be a 32-bit segment\n");
                    return 1;
                }

                assert(sg->segment_relative == 0);

                assert(sg->fragments != NULL);
                assert(sg->fragments_count > 0);
                assert(sg->fragments_count <= sg->fragments_alloc);
                frag = &sg->fragments[sg->fragments_count-1]; // should be the last one

                tsg = find_link_segment("__COMREL_RELOCTBL");
                if (tsg != NULL) {
                    assert(tsg->fragments != NULL);
                    assert(tsg->fragments_count > 0);
                    assert(tsg->fragments_count <= sg->fragments_alloc);
                    tfrag = &tsg->fragments[tsg->fragments_count-1]; // should be the last one

                    assert(tsg->segment_relative == 0);
                }
                else {
                    tfrag = NULL;
                }

                /* first, the relocation table */
                {
                    unsigned long old_init_ip,init_ip;
                    unsigned long ro,po;

                    if (tsg != NULL) {
                        ro = tfrag->offset;
                        assert((ro + tfrag->fragment_length) <= tsg->segment_length);

                        po = frag->offset;
                    }
                    else {
                        ro = frag->offset;
                        assert((ro + frag->fragment_length) <= sg->segment_length);

                        po = ro + (exe_relocation_table.size() * (size_t)2);
                    }

                    if (output_format == OFMT_DOSDRV) {
                        assert((po + sizeof(dosdrvrel_entry_point)) <= sg->segment_length);
                    }
                    else {
                        assert((po + sizeof(comrel_entry_point)) <= sg->segment_length);
                    }

                    sym = find_link_symbol("__COMREL_RELOC_TABLE",in_fileRefUndef,in_fileModuleRefUndef);
                    if (sym != NULL) return 1;
                    sym = new_link_symbol("__COMREL_RELOC_TABLE");
                    sym->in_file = in_fileRefInternal;
                    sym->groupdef = strdup("DGROUP");
                    if (tsg != NULL) {
                        sym->segdef = strdup("__COMREL_RELOCTBL");
                        sym->fragment = tsg->fragments_count-1;
                        sym->offset = ro - tfrag->offset;
                    }
                    else {
                        sym->segdef = strdup("__COMREL_RELOC");
                        sym->fragment = sg->fragments_count-1;
                        sym->offset = ro - frag->offset;
                    }

                    sym = find_link_symbol("__COMREL_RELOC_ENTRY",in_fileRefUndef,in_fileModuleRefUndef);
                    if (sym != NULL) return 1;
                    sym = new_link_symbol("__COMREL_RELOC_ENTRY");
                    sym->in_file = in_fileRefInternal;
                    sym->groupdef = strdup("DGROUP");
                    sym->segdef = strdup("__COMREL_RELOC");
                    sym->fragment = sg->fragments_count-1;
                    sym->offset = po - frag->offset;

                    /* do it */
                    assert(sg->image_ptr != NULL);

                    {
                        uint16_t *d = (uint16_t*)(sg->image_ptr + ro);
                        uint16_t *f = (uint16_t*)(sg->image_ptr + sg->segment_length);
                        struct exe_relocation *rel = &exe_relocation_table[0];
                        struct seg_fragment *frag;
                        struct link_segdef *lsg;
                        unsigned int reloc;
                        unsigned long roff;

                        if (tsg != NULL) {
                            d = (uint16_t*)(tsg->image_ptr + ro);
                            f = (uint16_t*)(tsg->image_ptr + sg->segment_length);
                        }

                        assert((d+exe_relocation_table.size()) <= f);
                        for (reloc=0;reloc < exe_relocation_table.size();reloc++,rel++) {
                            lsg = find_link_segment(rel->segname.c_str());
                            if (lsg == NULL) {
                                fprintf(stderr,"COM relocation entry refers to non-existent segment '%s'\n",rel->segname.c_str());
                                return 1;
                            }

                            assert(lsg->fragments != NULL);
                            assert(lsg->fragments_count != 0);
                            assert(lsg->fragments_count <= lsg->fragments_alloc);
                            assert(rel->fragment < lsg->fragments_count);
                            frag = lsg->fragments + rel->fragment;

                            roff = rel->offset + lsg->linear_offset + frag->offset + cmdoptions.com_segbase;

                            if (roff >= (0xFF00u - (exe_relocation_table.size() * (size_t)2u))) {
                                fprintf(stderr,"COM relocation entry is non-representable\n");
                                return 1;
                            }

                            d[reloc] = (uint16_t)roff;
                        }
                    }

                    if (output_format == OFMT_DOSDRV) {
                        uint8_t *d = (uint8_t*)(sg->image_ptr + po);
                        uint8_t *f = (uint8_t*)(sg->image_ptr + sg->segment_length);

                        assert((d+sizeof(dosdrvrel_entry_point)) <= f);
                        memcpy(d,dosdrvrel_entry_point,sizeof(dosdrvrel_entry_point));

                        *((uint16_t*)(d+dosdrvrel_entry_point_CX_COUNT)) = exe_relocation_table.size();

                        if (tsg != NULL)
                            *((uint16_t*)(d+dosdrvrel_entry_point_SI_OFFSET)) = ro + tsg->segment_offset;
                        else
                            *((uint16_t*)(d+dosdrvrel_entry_point_SI_OFFSET)) = ro + sg->segment_offset;

                        {
                            sym = find_link_symbol("__COMREL_RELOC_ENTRY_STRAT",in_fileRefUndef,in_fileModuleRefUndef);
                            if (sym != NULL) return 1;
                            sym = new_link_symbol("__COMREL_RELOC_ENTRY_STRAT");
                            sym->in_file = in_fileRefInternal;
                            sym->groupdef = strdup("DGROUP");
                            sym->segdef = strdup("__COMREL_RELOC");
                            sym->fragment = sg->fragments_count-1;
                            sym->offset = po - frag->offset;
                        }

                        {
                            sym = find_link_symbol("__COMREL_RELOC_ENTRY_INTR",in_fileRefUndef,in_fileModuleRefUndef);
                            if (sym != NULL) return 1;
                            sym = new_link_symbol("__COMREL_RELOC_ENTRY_INTR");
                            sym->in_file = in_fileRefInternal;
                            sym->groupdef = strdup("DGROUP");
                            sym->segdef = strdup("__COMREL_RELOC");
                            sym->fragment = sg->fragments_count-1;
                            sym->offset = po + dosdrvrel_entry_point_code_intr - frag->offset;
                        }

                        /* header handling will patch in mov DI fields later */
                    }
                    else {
                        uint8_t *d = (uint8_t*)(sg->image_ptr + po);
                        uint8_t *f = (uint8_t*)(sg->image_ptr + sg->segment_length);

                        if (entry_seg_link_target != NULL) {
                            struct seg_fragment *frag;

                            assert(entry_seg_link_target->fragments != NULL);
                            assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);
                            frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

                            old_init_ip = entry_seg_ofs + entry_seg_link_target->segment_offset + frag->offset;
                        }
                        else {
                            old_init_ip = 0x100;
                        }

                        init_ip = po + sg->segment_offset;

                        /* change entry point to new entry point */
                        if (cmdoptions.verbose) {
                            fprintf(stderr,"Old entry IP=0x%lx\n",old_init_ip);
                            fprintf(stderr,"New entry IP=0x%lx\n",init_ip);
                        }

                        if (map_fp != NULL) {
                            fprintf(map_fp,"\n");
                            fprintf(map_fp,"Entry point prior to replacement by relocation code:\n");
                            fprintf(map_fp,"---------------------------------------\n");

                            if (entry_seg_link_target != NULL) {
                                struct seg_fragment *frag;

                                assert(entry_seg_link_target->fragments != NULL);
                                assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);

                                frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

                                fprintf(map_fp,"  %04lx:%08lx %20s + 0x%08lx '%s':%u\n",
                                        entry_seg_link_target->segment_relative&0xfffful,
                                        entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs,
                                        entry_seg_link_target->name,
                                        frag->offset + entry_seg_ofs,
                                        get_in_file(frag->in_file),frag->in_module);

                            }

                            fprintf(map_fp,"\n");
                        }

                        assert((d+sizeof(comrel_entry_point)) <= f);
                        memcpy(d,comrel_entry_point,sizeof(comrel_entry_point));

                        *((uint16_t*)(d+comrel_entry_point_CX_COUNT)) = exe_relocation_table.size();

                        if (tsg != NULL)
                            *((uint16_t*)(d+comrel_entry_point_SI_OFFSET)) = ro + tsg->segment_offset;
                        else
                            *((uint16_t*)(d+comrel_entry_point_SI_OFFSET)) = ro + sg->segment_offset;

                        *((uint16_t*)(d+comrel_entry_point_JMP_ENTRY)) = old_init_ip - (init_ip + comrel_entry_point_JMP_ENTRY + 2);

                        cstr_free(&entry_seg_link_target_name);
                        entry_seg_link_target_fragment = (int)(frag - sg->fragments);
                        entry_seg_link_target_name = strdup(sg->name);
                        entry_seg_link_target = sg;
                        entry_seg_ofs = po - frag->offset;
                    }
                }
            }
        }
    }
 
    if (output_format == OFMT_COM) {
        struct link_segdef *sg;

        sg = find_link_segment("__COM_ENTRY_JMP");
        assert(sg != NULL);

        /* .COM require JMP instruction */
        if (entry_seg_link_target != NULL && com_entry_insert > 0) {
            struct seg_fragment *frag;
            unsigned long ofs;

            assert(sg->image_ptr != NULL);
            assert(sg->segment_length >= com_entry_insert);

            if (sg->segment_relative != 0) {
                fprintf(stderr,"__COM_ENTRY_JMP nonzero segment relative 0x%lx\n",sg->segment_relative);
                return 1;
            }
            if (sg->segment_base != cmdoptions.com_segbase) {
                fprintf(stderr,"__COM_ENTRY_JMP incorrect segment base 0x%lx\n",sg->segment_base);
                return 1;
            }
            if (sg->segment_offset != sg->segment_base) {
                fprintf(stderr,"__COM_ENTRY_JMP segment offset is not start of file, 0x%lx\n",sg->segment_offset);
                return 1;
            }

            assert(entry_seg_link_target->fragments != NULL);
            assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);
            frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

            ofs = (entry_seg_link_target->linear_offset+entry_seg_ofs+frag->offset);

            assert(com_entry_insert < 4);

            /* TODO: Some segments, like BSS or STACK, do not get emitted to disk.
             *       Except that in the COM format, segments can only be omitted
             *       from the end of the image. */

            if (com_entry_insert == 3) {
                assert(ofs <= (0xFFFFu - 3u));
                sg->image_ptr[0] = 0xE9; /* JMP near */
                *((uint16_t*)(sg->image_ptr+1)) = (uint16_t)ofs - 3;
            }
            else if (com_entry_insert == 2) {
                assert(ofs <= (0x7Fu + 2u));
                sg->image_ptr[0] = 0xEB; /* JMP short */
                sg->image_ptr[1] = (unsigned char)ofs - 2;
            }
            else {
                abort();
            }
        }
    }

    dump_link_relocations();
    dump_link_symbols();
    dump_link_segments();

    qsort(link_symbols, link_symbols_count, sizeof(struct link_symbol), link_symbol_qsort_cmp);

    /* write output */
    assert(!cmdoptions.out_file.empty());
    {
        int fd;

        fd = open(cmdoptions.out_file.c_str(),O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
        if (fd < 0) {
            fprintf(stderr,"Unable to open output file\n");
            return 1;
        }

        if (output_format == OFMT_EXE || output_format == OFMT_DOSDRVEXE) {
            /* EXE header */
            unsigned char tmp[32];
            unsigned long disk_size = 0;
            unsigned long stack_size = 0;
            unsigned long header_size = 0;
            unsigned long o_header_size = 0;
            unsigned long resident_size = 0;
            unsigned long relocation_table_offset = 0;
            unsigned long max_resident_size = 0xFFFF0ul; /* by default, take ALL memory */
            unsigned long init_ss = 0,init_sp = 0,init_cs = 0,init_ip = 0;
            unsigned int i,ofs;

            if (link_segments_count > 0) {
                struct link_segdef *sd = &link_segments[0];
                header_size = sd->file_offset;
            }
            o_header_size = header_size;

            if (!exe_relocation_table.empty()) {
                assert(header_size >= 0x20);
                relocation_table_offset = header_size;
                header_size += (exe_relocation_table.size() * (size_t)4ul);
            }

            /* header_size must be a multiple of 16 */
            if (header_size % 16ul)
                header_size += 16ul - (header_size % 16ul);

            /* move segments farther down if needed */
            if (o_header_size < header_size) {
                unsigned long adj = header_size - o_header_size;

                for (i=0;i < link_segments_count;i++) {
                    struct link_segdef *sd = &link_segments[i];

                    if (sd->segment_length == 0) continue;

                    sd->file_offset += adj;
                }
            }

            for (i=0;i < link_segments_count;i++) {
                struct link_segdef *sd = &link_segments[i];

                if (sd->segment_length == 0) continue;

                ofs = sd->linear_offset + sd->segment_length + header_size;
                if (resident_size < ofs) resident_size = ofs;

                if (!sd->noemit) {
                    ofs = sd->file_offset + sd->segment_length;
                    if (disk_size < ofs) disk_size = ofs;
                }
            }

            {
                unsigned long ofs;
                struct link_segdef *stacksg = find_link_segment_by_class_last("STACK");

                if (stacksg != NULL) {
                    init_ss = stacksg->segment_relative;
                    init_sp = stacksg->segment_offset + stacksg->segment_length;
                    stack_size = stacksg->segment_length;
                }
                else {
                    fprintf(stderr,"Warning, no STACK class segment defined\n");

                    if (map_fp != NULL)
                        fprintf(map_fp,"* Warning, no STACK class segment defined\n");

                    init_ss = 0;
                    init_sp = resident_size + cmdoptions.want_stack_size;
                    stack_size = cmdoptions.want_stack_size;
                    while (init_sp > 0xFF00ul) {
                        init_sp -= 0x100;
                        init_ss += 0x10;
                    }
                }

                ofs = (init_ss << 4ul) + init_sp + header_size;
                if (resident_size < ofs) resident_size = ofs;
            }

            if (cmdoptions.verbose) {
                fprintf(stderr,"EXE header:                    0x%lx\n",header_size);
                fprintf(stderr,"EXE resident size with header: 0x%lx\n",resident_size);
                fprintf(stderr,"EXE resident size:             0x%lx\n",resident_size - header_size);
                fprintf(stderr,"EXE disk size without header:  0x%lx\n",disk_size - header_size);
                fprintf(stderr,"EXE disk size:                 0x%lx\n",disk_size);
            }

            if (map_fp != NULL) {
                fprintf(map_fp,"\n");

                fprintf(map_fp,"EXE header stats:\n");
                fprintf(map_fp,"---------------------------------------\n");

                fprintf(map_fp,"EXE header:                    0x%lx\n",header_size);
                fprintf(map_fp,"EXE resident size with header: 0x%lx\n",resident_size);
                fprintf(map_fp,"EXE resident size:             0x%lx\n",resident_size - header_size);
                fprintf(map_fp,"EXE disk size without header:  0x%lx\n",disk_size - header_size);
                fprintf(map_fp,"EXE disk size:                 0x%lx\n",disk_size);
                fprintf(map_fp,"EXE stack size:                0x%lx\n",stack_size);
                fprintf(map_fp,"EXE stack pointer:             %04lx:%04lx [0x%08lx]\n",
                    init_ss,init_sp,(init_ss << 4ul) + init_sp);
 
                fprintf(map_fp,"\n");
            }

            /* entry point */
            if (entry_seg_link_target != NULL && entry_seg_link_frame != NULL) {
                struct seg_fragment *frag;

                assert(entry_seg_link_target->fragments != NULL);
                assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);
                frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

                if (entry_seg_link_target->segment_base != entry_seg_link_frame->segment_base) {
                    fprintf(stderr,"EXE Entry point with frame != target not yet supported\n");
                    return 1;
                }

                init_cs = entry_seg_link_target->segment_relative;
                init_ip = entry_seg_ofs + entry_seg_link_target->segment_offset + frag->offset;

                if (cmdoptions.verbose)
                    fprintf(stderr,"EXE entry: %04lx:%04lx in %s\n",init_cs,init_ip,entry_seg_link_target->name);
            }
            else {
                if (output_format == OFMT_DOSDRVEXE) {
                    fprintf(stderr,"EXE warning: An entry point is recommended even for MS-DOS EXE-type device drivers.\n");
                    fprintf(stderr,"             Normally such EXEs are intended to be both a runnable command and device\n");
                    fprintf(stderr,"             driver. One common example: EMM386.EXE. Please define an entry point to\n");
                    fprintf(stderr,"             avoid this warning.\n");
                }
                else {
                    fprintf(stderr,"EXE warning: No entry point. Executable will likely crash executing\n");
                    fprintf(stderr,"             code or data at the start of the image. Please define\n");
                    fprintf(stderr,"             an entry point routine to avoid that.\n");
               }
            }

            if (cmdoptions.verbose)
                fprintf(stderr,"EXE header size: %lu\n",header_size);

            assert(resident_size >= disk_size);
            assert(header_size != 0u);
            assert((header_size % 16u) == 0u);
            assert(sizeof(tmp) >= 32);
            memset(tmp,0,32);
            tmp[0] = 'M';
            tmp[1] = 'Z';

            if (cmdoptions.verbose) {
                fprintf(stderr,"Adjusted segment table\n");
                dump_link_segments();
            }
 
            {
                unsigned int blocks,lastblock;

                blocks = disk_size / 512u;
                lastblock = disk_size % 512u;
                if (lastblock != 0) blocks++;

                *((uint16_t*)(tmp+2)) = lastblock;
                *((uint16_t*)(tmp+4)) = blocks;
                // no relocations (yet)
                *((uint16_t*)(tmp+8)) = header_size / 16u; /* in paragraphs */
                *((uint16_t*)(tmp+10)) = ((resident_size + 15ul - disk_size) / 16ul); /* in paragraphs, additional memory needed */
                *((uint16_t*)(tmp+12)) = (max_resident_size + 15ul) / 16ul; /* maximum additional memory */
                *((uint16_t*)(tmp+14)) = init_ss; /* relative */
                *((uint16_t*)(tmp+16)) = init_sp;
                // no checksum
                *((uint16_t*)(tmp+20)) = init_ip;
                *((uint16_t*)(tmp+22)) = init_cs; /* relative */
                // no relocation table (yet)
                // no overlay

                if (!exe_relocation_table.empty()) {
                    *((uint16_t*)(tmp+24)) = (uint16_t)relocation_table_offset;
                    *((uint16_t*)(tmp+6)) = (uint16_t)exe_relocation_table.size();
                }
            }

            if (lseek(fd,0,SEEK_SET) == 0) {
                if (write(fd,tmp,32) == 32) {
                    /* good */
                }
            }

            if (!exe_relocation_table.empty()) {
                if ((unsigned long)lseek(fd,relocation_table_offset,SEEK_SET) == relocation_table_offset) {
                    struct exe_relocation *rel = &exe_relocation_table[0];
                    struct seg_fragment *frag;
                    struct link_segdef *lsg;
                    unsigned long rseg,roff;
                    unsigned int reloc;

                    for (reloc=0;reloc < exe_relocation_table.size();reloc++,rel++) {
                        lsg = find_link_segment(rel->segname.c_str());
                        if (lsg == NULL) {
                            fprintf(stderr,"COM relocation entry refers to non-existent segment '%s'\n",rel->segname.c_str());
                            return 1;
                        }

                        assert(lsg->fragments != NULL);
                        assert(lsg->fragments_count != 0);
                        assert(lsg->fragments_count <= lsg->fragments_alloc);
                        assert(rel->fragment < lsg->fragments_count);
                        frag = lsg->fragments + rel->fragment;

                        rseg = 0;
                        roff = rel->offset + lsg->linear_offset + frag->offset;

                        while (roff >= 0x4000ul) {
                            rseg += 0x10ul;
                            roff -= 0x100ul;
                        }

                        *((uint16_t*)(tmp + 0)) = (uint16_t)roff;
                        *((uint16_t*)(tmp + 2)) = (uint16_t)rseg;
                        write(fd,tmp,4);
                    }
                }
            }
        }
        if (output_format == OFMT_DOSDRV || output_format == OFMT_DOSDRVEXE) {
            /* the entry point symbol must exist and must be at the very start of the file,
             * or at the very beginning of the resident image if EXE */
            struct link_segdef *segdef;
            struct seg_fragment *frag;
            struct link_symbol *sym;
            unsigned long ofs;

            sym = find_link_symbol(cmdoptions.dosdrv_header_symbol.c_str(),in_fileRefUndef,in_fileModuleRefUndef);
            if (sym == NULL) {
                fprintf(stderr,"Required symbol '%s' not found (MS-DOS .SYS header)\n",cmdoptions.dosdrv_header_symbol.c_str());
                return 1;
            }
            assert(sym->name != NULL);

            segdef = find_link_segment(sym->segdef);
            if (segdef == NULL) {
                fprintf(stderr,"Required symbol '%s' not found (MS-DOS .SYS header) missing SEGDEF '%s'\n",
                    cmdoptions.dosdrv_header_symbol.c_str(),sym->segdef);
                return 1;
            }

            assert(segdef->fragments != NULL);
            assert(segdef->fragments_count <= segdef->fragments_alloc);
            assert(sym->fragment < segdef->fragments_count);
            frag = &segdef->fragments[sym->fragment];

            ofs = sym->offset + frag->offset;
            if (ofs != 0ul) {
                fprintf(stderr,"Required symbol '%s' not found (MS-DOS .SYS header) has nonzero offset 0x%lx within segment '%s'\n",
                    cmdoptions.dosdrv_header_symbol.c_str(),ofs,sym->segdef);
                return 1;
            }

            if (segdef->linear_offset != 0ul) {
                fprintf(stderr,"Required symbol '%s' not found (MS-DOS .SYS header) starts within segment '%s' which is not at the start of the file (offset 0x%lx)\n",
                    cmdoptions.dosdrv_header_symbol.c_str(),sym->segdef,segdef->linear_offset);
                return 1;
            }

            if (map_fp != NULL) {
                char tmp[9];
                unsigned int i;
                unsigned char *hdr_p;

                hdr_p = segdef->image_ptr + ofs;

                fprintf(map_fp,"\n");
                fprintf(map_fp,"MS-DOS device driver information:\n");
                fprintf(map_fp,"---------------------------------------\n");

                fprintf(map_fp,"  Attributes:          0x%04x\n",*((uint16_t*)(hdr_p + 0x4)));
                fprintf(map_fp,"  Strategy routine:    0x%04x\n",*((uint16_t*)(hdr_p + 0x6)));
                fprintf(map_fp,"  Interrupt routine:   0x%04x\n",*((uint16_t*)(hdr_p + 0x8)));

                memcpy(tmp,hdr_p + 0xA,8); tmp[8] = 0;
                for (i=0;i < 8;i++) {
                    if (tmp[i] < 32 || tmp[i] >= 127) tmp[i] = ' ';
                }
                fprintf(map_fp,"  Initial device name: '%s'\n",tmp);

                fprintf(map_fp,"\n");
            }

            if (output_format == OFMT_DOSDRV && output_format_variant == OFMTVAR_COMREL && !exe_relocation_table.empty()) {
                unsigned char *hdr_p;
                unsigned char *reloc_p;
                struct link_segdef *rsegdef;
                struct seg_fragment *rfrag;
                struct link_symbol *rsym;
                unsigned long rofs;

                rsym = find_link_symbol("__COMREL_RELOC_ENTRY",in_fileRefUndef,in_fileModuleRefUndef);
                assert(rsym != NULL);
                assert(rsym->segdef != NULL);

                rsegdef = find_link_segment(rsym->segdef);
                assert(rsegdef != NULL);

                assert(rsegdef->fragments != NULL);
                assert(rsegdef->fragments_count <= rsegdef->fragments_alloc);
                assert(rsym->fragment < rsegdef->fragments_count);
                rfrag = &rsegdef->fragments[rsym->fragment];

                rofs = rsym->offset + rfrag->offset;

                assert(rsegdef->image_ptr != NULL);
                assert((rofs + sizeof(dosdrvrel_entry_point)) <= rsegdef->segment_length);

                assert(segdef->image_ptr != NULL);
                assert((ofs + 10) <= segdef->segment_length);
                assert(ofs == 0);

                hdr_p = segdef->image_ptr + ofs;
                reloc_p = rsegdef->image_ptr + rofs;

                if (cmdoptions.verbose) {
                    fprintf(stderr,"Original entry: 0x%x, 0x%x\n",
                        *((uint16_t*)(hdr_p + 0x06)),
                        *((uint16_t*)(hdr_p + 0x08)));
                }

                /* copy entry points (2) to the relocation parts of the ASM we inserted */
                *((uint16_t*)(reloc_p + dosdrvrel_entry_point_orig_entry1)) = *((uint16_t*)(hdr_p + 0x06));
                *((uint16_t*)(reloc_p + dosdrvrel_entry_point_entry1)) = *((uint16_t*)(hdr_p + 0x06));
                *((uint16_t*)(hdr_p + 0x06)) = rofs + rsegdef->segment_offset + dosdrvrel_entry_point_entry1 - 1;

                *((uint16_t*)(reloc_p + dosdrvrel_entry_point_orig_entry2)) = *((uint16_t*)(hdr_p + 0x08));
                *((uint16_t*)(reloc_p + dosdrvrel_entry_point_entry2)) = *((uint16_t*)(hdr_p + 0x08));
                *((uint16_t*)(hdr_p + 0x08)) = rofs + rsegdef->segment_offset + dosdrvrel_entry_point_entry2 - 1;

                if (cmdoptions.verbose) {
                    fprintf(stderr,"New entry: 0x%x, 0x%x\n",
                        *((uint16_t*)(hdr_p + 0x06)),
                        *((uint16_t*)(hdr_p + 0x08)));
                }

                if (map_fp != NULL) {
                    fprintf(map_fp,"\n");
                    fprintf(map_fp,"MS-DOS device driver information, after relocation table added:\n");
                    fprintf(map_fp,"---------------------------------------\n");

                    fprintf(map_fp,"  Strategy routine:    0x%04x\n",*((uint16_t*)(hdr_p + 0x6)));
                    fprintf(map_fp,"  Interrupt routine:   0x%04x\n",*((uint16_t*)(hdr_p + 0x8)));

                    fprintf(map_fp,"\n");
                }
            }
        }

        {
            unsigned int linkseg;

            for (linkseg=0;linkseg < link_segments_count;linkseg++) {
                struct link_segdef *sd = &link_segments[linkseg];

                if (sd->segment_length == 0 || sd->noemit) continue;

                if ((unsigned long)lseek(fd,sd->file_offset,SEEK_SET) != sd->file_offset) {
                    fprintf(stderr,"Seek error\n");
                    return 1;
                }

                assert(sd->image_ptr != NULL);
                if ((unsigned long)write(fd,sd->image_ptr,sd->segment_length) != sd->segment_length) {
                    fprintf(stderr,"Write error\n");
                    return 1;
                }
            }
        }

        if (!cmdoptions.hex_output.empty()) {
            unsigned char tmp[16];
            long sz,count=0;
            FILE *hfp;
            int rd,x;

            {
                string::iterator i = cmdoptions.out_file.begin();

                while (i != cmdoptions.out_file.end()) {
                    char c = *(i++);

                    if (isalpha(c) || isdigit(c) || c == '_') {
                        if (i == cmdoptions.out_file.begin() && isdigit(c)) /* symbols cannot start with digits */
                            hex_output_name += '_';

                        hex_output_name += c;
                    }
                    else {
                        hex_output_name += '_';
                    }
                }
            }

            sz = lseek(fd,0,SEEK_END);

            if (cmdoptions.hex_split)
                hex_output_tmpfile = cmdoptions.hex_output + "." + (cmdoptions.hex_cpp ? "cpp" : "c");
            else
                hex_output_tmpfile = cmdoptions.hex_output;

            hfp = fopen(hex_output_tmpfile.c_str(),"w");
            if (hfp == NULL) {
                fprintf(stderr,"Unable to write hex output\n");
                return 1;
            }

            fprintf(hfp,"const uint8_t %s_bin[%lu] = {\n",hex_output_name.c_str(),sz);

            count = 0;
            lseek(fd,0,SEEK_SET);
            while ((rd=read(fd,tmp,sizeof(tmp))) > 0) {
                fprintf(hfp,"    ");
                for (x=0;x < rd;x++) {
                    fprintf(hfp,"0x%02x",tmp[x]);
                    if ((count+x+1l) < sz) fprintf(hfp,",");
                }
                fprintf(hfp," /* 0x%08lx */\n",(unsigned long)count);

                count += (unsigned int)rd;
            }

            fprintf(hfp,"};\n");

            if (cmdoptions.hex_split) {
                fclose(hfp);

                hex_output_tmpfile = cmdoptions.hex_output + ".h";

                hfp = fopen(hex_output_tmpfile.c_str(),"w");
                if (hfp == NULL) {
                    fprintf(stderr,"Unable to write hex output\n");
                    return 1;
                }

                fprintf(hfp,"extern const uint8_t %s_bin[%lu];\n",hex_output_name.c_str(),sz);
            }

            fprintf(hfp,"#define %s_bin_sz (%ldul)\n",hex_output_name.c_str(),(unsigned long)sz);

            dump_hex_segments(hfp, hex_output_name.c_str());
            dump_hex_symbols(hfp, hex_output_name.c_str());

            fclose(hfp);
        }

        close(fd);
    }

    if (map_fp != NULL) {
        fprintf(map_fp,"\n");
        fprintf(map_fp,"Entry point:\n");
        fprintf(map_fp,"---------------------------------------\n");

        if (entry_seg_link_target != NULL) {
            unsigned int symi = 0,fsymi = ~0u;
            unsigned long sofs,cofs;
            struct link_symbol *sym;
            struct seg_fragment *frag;
            struct seg_fragment *sfrag;
            struct link_segdef *ssg;

            assert(entry_seg_link_target->fragments != NULL);
            assert(entry_seg_link_target_fragment < entry_seg_link_target->fragments_count);

            frag = &entry_seg_link_target->fragments[entry_seg_link_target_fragment];

            fprintf(map_fp,"  %04lx:%08lx %20s + 0x%08lx '%s':%u\n",
                entry_seg_link_target->segment_relative&0xfffful,
                entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs,
                entry_seg_link_target->name,
                frag->offset + entry_seg_ofs,
                get_in_file(frag->in_file),frag->in_module);

            while (symi < link_symbols_count) {
                sym = &link_symbols[symi++];

                assert(sym->segdef != NULL);
                if (strcmp(sym->segdef, entry_seg_link_target->name)) continue;

                ssg = find_link_segment(sym->segdef);
                assert(ssg != NULL);

                assert(ssg->fragments != NULL);
                assert(ssg->fragments_count <= ssg->fragments_alloc);
                assert(sym->fragment < ssg->fragments_count);

                sfrag = &ssg->fragments[sym->fragment];

                sofs = ssg->segment_offset + sfrag->offset + sym->offset;
                cofs = entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs;

                if (sofs > cofs) break;
                else fsymi = symi - 1u;
            }

            if (fsymi != (~0u)) {
                sym = &link_symbols[fsymi];

                assert(sym->segdef != NULL);
                assert(strcmp(sym->segdef, entry_seg_link_target->name) == 0);

                ssg = find_link_segment(sym->segdef);
                assert(ssg != NULL);

                assert(ssg->fragments != NULL);
                assert(ssg->fragments_count <= ssg->fragments_alloc);
                assert(sym->fragment < ssg->fragments_count);

                sfrag = &ssg->fragments[sym->fragment];

                sofs = ssg->segment_offset + sfrag->offset + sym->offset;
                cofs = entry_seg_link_target->segment_offset + frag->offset + entry_seg_ofs;

                fprintf(map_fp,"    %s + 0x%08lx\n",sym->name,cofs - sofs);
            }
        }
        else {
            fprintf(map_fp,"  No entry point defined\n");
        }

        fprintf(map_fp,"\n");
    }

    cstr_free(&entry_seg_link_target_name);
    cstr_free(&entry_seg_link_frame_name);

    if (map_fp != NULL) {
        fclose(map_fp);
        map_fp = NULL;
    }

    link_symbols_free();
    free_link_segments();
    free_exe_relocations();
    return 0;
}

