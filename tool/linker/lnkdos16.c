/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

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

#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

enum {
    PASS_GATHER,
    PASS_BUILD,
    PASS_MAX
};

//================================== PROGRAM ================================

#define MAX_GROUPS                      256

#define MAX_IN_FILES                    256

static char*                            out_file = NULL;

static char*                            in_file[MAX_IN_FILES];
static unsigned int                     in_file_count = 0;
static unsigned int                     current_in_file = 0;
static unsigned int                     current_in_mod = 0;

static unsigned char                    do_dosseg = 1;

struct omf_context_t*                   omf_state = NULL;

static unsigned char                    verbose = 0;

/* NTS: Default -com100, use -com0 for Open Watcom compiled C source */
static unsigned short                   com_segbase = (unsigned short)(~0u);

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

enum {
    OFMTVAR_NONE=0,

    OFMTVAR_COMREL=10
};

enum {
    OFMT_COM=0,
    OFMT_EXE
};

struct exe_relocation {
    char*                               segname;
    unsigned int                        fragment;
    unsigned long                       offset;
};

static struct exe_relocation*           exe_relocation_table = NULL;
static size_t                           exe_relocation_table_count = 0;
static size_t                           exe_relocation_table_alloc = 0;

struct exe_relocation *new_exe_relocation(void) {
    if (exe_relocation_table == NULL) {
        exe_relocation_table_count = 0;
        exe_relocation_table_alloc = 4096;
        exe_relocation_table = (struct exe_relocation*)malloc(sizeof(struct exe_relocation) * exe_relocation_table_alloc);
        if (exe_relocation_table == NULL) return NULL;
    }

    if (exe_relocation_table_count >= exe_relocation_table_alloc)
        return NULL;

    return exe_relocation_table + (exe_relocation_table_count++);
}

void free_exe_relocation_entry(struct exe_relocation *r) {
    if (r->segname != NULL) {
        free(r->segname);
        r->segname = NULL;
    }
}

void free_exe_relocations(void) {
    unsigned int i;

    if (exe_relocation_table) {
        for (i=0;i < exe_relocation_table_count;i++) free_exe_relocation_entry(exe_relocation_table + i);
        free(exe_relocation_table);
        exe_relocation_table = NULL;
        exe_relocation_table_count = 0;
        exe_relocation_table_alloc = 0;
    }
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
    unsigned short                      in_file;
    unsigned short                      in_module;
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
        struct link_symbol *n = realloc(link_symbols, sz * sizeof(struct link_symbol));
        if (n == NULL) return -1;
        link_symbols = n;
        assert(old_alloc != 0);
        assert(old_alloc < sz);
        memset(link_symbols + old_alloc, 0, (sz - old_alloc) * sizeof(struct link_symbol));
    }
    else {
        link_symbols = malloc(sz * sizeof(struct link_symbol));
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
    }

    return sym;
}

struct link_symbol *find_link_symbol(const char *name) {
    struct link_symbol *sym;
    size_t i = 0;

    if (link_symbols != NULL) {
        for (;i < link_symbols_count;i++) {
            sym = link_symbols + i;
            if (sym->name != NULL) {
                if (!strcmp(sym->name, name))
                    return sym;
            }
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
    unsigned short                      in_file;
    unsigned short                      in_module;
    unsigned short                      segidx;
    unsigned long                       offset;
    unsigned long                       fragment_length;
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
}

void owlink_stack_bss_arrange(void) {
    unsigned int s = 0,e = link_segments_count - 1u;

    if (link_segments_count == 0) return;
    if (output_format == OFMT_COM || output_format == OFMT_EXE) {
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
        sg->fragments = calloc(sg->fragments_alloc, sizeof(struct seg_fragment));
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

    if (exe_relocation_table == NULL) return;

    while (i < exe_relocation_table_count) {
        struct exe_relocation *rel = &exe_relocation_table[i++];

        assert(rel->segname != NULL);
        fprintf(stderr,"relocation[%u]: seg='%s' frag=%u offset=0x%lx\n",
            i,rel->segname,rel->fragment,rel->offset);
    }
}

void dump_link_symbols(void) {
    unsigned int i=0;

    if (link_symbols == NULL) return;

    while (i < link_symbols_count) {
        struct link_symbol *sym = &link_symbols[i++];
        if (sym->name == NULL) continue;

        fprintf(stderr,"symbol[%u]: name='%s' group='%s' seg='%s' offset=0x%lx file='%s' module=%u local=%u\n",
            i/*post-increment, intentional*/,sym->name,sym->groupdef,sym->segdef,sym->offset,
            in_file[sym->in_file],sym->in_module,sym->is_local);
    }
}

void dump_link_segments(void) {
    unsigned int i=0,f;

    while (i < link_segments_count) {
        struct link_segdef *sg = &link_segments[i++];

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

        if (sg->fragments != NULL) {
            for (f=0;f < sg->fragments_count;f++) {
                struct seg_fragment *frag = &sg->fragments[f];

                fprintf(stderr,"  fragment[%u]: file='%s' module=%u offset=0x%lx length=0x%lx segidx=%u\n",
                    f,in_file[frag->in_file],frag->in_module,frag->offset,frag->fragment_length,frag->segidx);
            }
        }
    }
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

    if (verbose)
        fprintf(stderr,"LEDATA '%s' base=0x%lx offset=0x%lx len=%lu enumo=0x%lx in frag ofs=0x%lx\n",
                segname,lsg->load_base,max_ofs,info->data_length,info->enum_data_offset,frag->offset);

    return 0;
}

int fixupp_get(struct omf_context_t *omf_state,unsigned long *fseg,unsigned long *fofs,struct link_segdef **sdef,const struct omf_fixupp_t *ent,unsigned int method,unsigned int index) {
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

        sym = find_link_symbol(defname);
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
            if (fixupp_get(omf_state,&frame_seg,&frame_ofs,&frame_sdef,ent,ent->frame_method,ent->frame_index))
                return -1;
            if (fixupp_get(omf_state,&targ_seg,&targ_ofs,&targ_sdef,ent,ent->target_method,ent->target_index))
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

                if (output_format == OFMT_COM) {
                    if (output_format_variant == OFMTVAR_COMREL) {
                        if (pass == PASS_GATHER) {
                            struct exe_relocation *reloc = new_exe_relocation();
                            if (reloc == NULL) {
                                fprintf(stderr,"Unable to allocate relocation\n");
                                return -1;
                            }

                            assert(current_link_segment->fragments_read > 0);
                            assert(current_link_segment->name != NULL);
                            reloc->segname = strdup(current_link_segment->name);
                            reloc->fragment = current_link_segment->fragments_read - 1u;
                            reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset;

                            if (verbose)
                                fprintf(stderr,"COM relocation entry: Patch up %s:%u:%04lx\n",reloc->segname,reloc->fragment,reloc->offset);
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
                else if (output_format == OFMT_EXE) {
                    /* emit as a relocation */
                    if (pass == PASS_GATHER) {
                        struct exe_relocation *reloc = new_exe_relocation();
                        if (reloc == NULL) {
                            fprintf(stderr,"Unable to allocate relocation\n");
                            return -1;
                        }

                        assert(current_link_segment->fragments_read > 0);
                        assert(current_link_segment->name != NULL);
                        reloc->segname = strdup(current_link_segment->name);
                        reloc->fragment = current_link_segment->fragments_read - 1u;
                        reloc->offset = ent->omf_rec_file_enoffs + ent->data_record_offset;

                        if (verbose)
                            fprintf(stderr,"EXE relocation entry: Patch up %s:%u:%04lx\n",reloc->segname,reloc->fragment,reloc->offset);
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

        if (verbose)
            fprintf(stderr,"pubdef[%u]: '%s' group='%s' seg='%s' offset=0x%lx finalofs=0x%lx local=%u\n",
                    first,name,groupname,segname,(unsigned long)pubdef->public_offset,pubdef->public_offset + lsg->load_base,is_local);

        sym = find_link_symbol(name);
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
                if (verbose)
                    fprintf(stderr,"SEGDEF class='%s' name='%s' already exits\n",classname,name);

                lsg->attr.f.f.alignment = sg->attr.f.f.alignment;
                if (lsg->attr.f.f.use32 != sg->attr.f.f.use32 ||
                    lsg->attr.f.f.combination != sg->attr.f.f.combination ||
                    lsg->attr.f.f.big_segment != sg->attr.f.f.big_segment) {
                    fprintf(stderr,"ERROR, segment attribute changed\n");
                    return -1;
                }
            }
            else {
                if (verbose)
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
            }

            if (verbose)
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
    fprintf(stderr,"  -of <fmt>    Output format (COM, EXE, COMREL)\n");
    fprintf(stderr,"                COM = flat COM executable\n");
    fprintf(stderr,"                COMREL = flat COM executable, relocatable\n");
    fprintf(stderr,"                EXE = segmented EXE executable\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
    fprintf(stderr,"  -no-dosseg   No DOSSEG sort order\n");
    fprintf(stderr,"  -dosseg      DOSSEG sort order\n");
    fprintf(stderr,"  -comNNN      Link .COM segment starting at 0xNNN\n");
    fprintf(stderr,"  -com100      Link .COM segment starting at 0x100\n");
    fprintf(stderr,"  -com0        Link .COM segment starting at 0 (Watcom Linker)\n");
    fprintf(stderr,"  -pflat       Prefer .COM-like flat layout\n");
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

    if (verbose)
        printf("----END-----\n");
}

int main(int argc,char **argv) {
    unsigned char diddump = 0;
    unsigned char pass;
    unsigned int inf;
    int i,fd,ret;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                if (in_file_count >= MAX_IN_FILES) {
                    fprintf(stderr,"Too many input files\n");
                    return 1;
                }

                in_file[in_file_count] = argv[i++];
                if (in_file[in_file_count] == NULL) return 1;
                in_file_count++;
            }
            else if (!strcmp(a,"pflat")) {
                prefer_flat = 1;
            }
            else if (!strncmp(a,"com",3)) {
                a += 3;
                if (!isxdigit(*a)) return 1;
                com_segbase = strtoul(a,NULL,16);
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
                else {
                    fprintf(stderr,"Unknown format\n");
                    return 1;
                }
            }
            else if (!strcmp(a,"o")) {
                out_file = argv[i++];
                if (out_file == NULL) return 1;
            }
            else if (!strcmp(a,"v")) {
                verbose = 1;
            }
            else if (!strcmp(a,"dosseg")) {
                do_dosseg = 1;
            }
            else if (!strcmp(a,"no-dosseg")) {
                do_dosseg = 0;
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

    if (com_segbase == (unsigned short)(~0u)) {
        if (output_format == OFMT_COM) {
            com_segbase = 0x100;
        }
        else if (output_format == OFMT_EXE) {
            com_segbase = 0;
        }
    }

    if (in_file_count == 0) {
        help();
        return 1;
    }

    if (out_file == NULL) {
        help();
        return 1;
    }

    for (pass=0;pass < PASS_MAX;pass++) {
        for (inf=0;inf < in_file_count;inf++) {
            assert(in_file[inf] != NULL);

            fd = open(in_file[inf],O_RDONLY|O_BINARY);
            if (fd < 0) {
                fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
                return 1;
            }
            current_in_file = inf;

            // prepare parsing
            if ((omf_state=omf_context_create()) == NULL) {
                fprintf(stderr,"Failed to init OMF parsing state\n");
                return 1;
            }
            omf_state->flags.verbose = (verbose > 0);

            diddump = 0;
            current_in_mod = 0;
            omf_context_begin_file(omf_state);

            do {
                ret = omf_context_read_fd(omf_state,fd);
                if (ret == 0) {
                    if (apply_FIXUPP(omf_state,0,inf,current_in_mod,pass))
                        return 1;
                    omf_fixupps_context_free_entries(&omf_state->FIXUPPs);

                    if (omf_record_is_modend(&omf_state->record)) {
                        if (!diddump && verbose) {
                            my_dumpstate(omf_state);
                            diddump = 1;
                        }

                        if (verbose)
                            printf("----- next module -----\n");

                        ret = omf_context_next_lib_module_fd(omf_state,fd);
                        if (ret < 0) {
                            printf("Unable to advance to next .LIB module, %s\n",strerror(errno));
                            if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
                        }
                        else if (ret > 0) {
                            current_in_mod++;
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

                            if (pass == PASS_GATHER && pubdef_add(omf_state, p_count, omf_state->record.rectype, inf, current_in_mod, pass))
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

                            if (segdef_add(omf_state, p_count, inf, current_in_mod, pass))
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

                                if (verbose) {
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

                                    if (verbose)
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

            if (!diddump && verbose) {
                my_dumpstate(omf_state);
                diddump = 1;
            }

            if (apply_FIXUPP(omf_state,0,inf,current_in_mod,pass))
                return 1;
            omf_fixupps_context_free_entries(&omf_state->FIXUPPs);

            omf_context_clear(omf_state);
            omf_state = omf_context_destroy(omf_state);

            close(fd);
        }

        if (pass == PASS_GATHER) {
            unsigned long file_baseofs = 0;

            owlink_default_sort_seg();

            if (do_dosseg)
                owlink_dosseg_sort_order();

            owlink_stack_bss_arrange();

            /* entry point checkup */
            if (entry_seg_link_target == NULL) {
                fprintf(stderr,"WARNING: No entry point found\n");
            }

            /* put segments in order, linear offset */
            {
                unsigned long ofs = 0;
                unsigned long m;

                /* .COM format: if the entry point is nonzero, a JMP instruction
                 * must be inserted at the start to JMP to the entry point */
                if (output_format == OFMT_EXE) {
                    /* EXE */
                    /* TODO: relocation table */
                    file_baseofs = 32;
                }
                else if (output_format == OFMT_COM) {
                    /* COM relocatable */
                    if (output_format_variant == OFMTVAR_COMREL) {
                        /* always make room */
                        com_entry_insert = 3;
                        ofs += com_entry_insert;

                        if (verbose)
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

                            ofs += com_entry_insert;
                        }
                    }
                }
                else {
                    abort();
                }

                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    if (sd->initial_alignment != 0ul) {
                        m = ofs % sd->initial_alignment;
                        if (m != 0ul) ofs += sd->initial_alignment - m;
                    }

                    if (verbose)
                        fprintf(stderr,"segment[%u] ofs=0x%lx len=0x%lx\n",
                                inf,ofs,sd->segment_length);

                    if (output_format == OFMT_COM || output_format == OFMT_EXE) {
                        if (sd->segment_length > 0x10000ul) {
                            dump_link_segments();
                            fprintf(stderr,"Segment too large >= 64KB\n");
                            return -1;
                        }
                    }

                    sd->linear_offset = ofs;
                    ofs += sd->segment_length;
                }
            }

            /* if a .COM executable, then all segments are arranged so that the first byte
             * is at 0x100 */
            if (output_format == OFMT_EXE) {
                unsigned long segrel = 0;
                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];
                    struct link_segdef *gd = sd->groupname != NULL ? find_link_segment_by_grpdef(sd->groupname) : 0;
                    struct link_segdef *cd = sd->classname != NULL ? find_link_segment_by_class(sd->classname) : 0;

                    if (gd != NULL)
                        segrel = gd->linear_offset >> 4ul;
                    else if (cd != NULL)
                        segrel = cd->linear_offset >> 4ul;
                    else
                        segrel = sd->linear_offset >> 4ul;

                    if (prefer_flat && sd->linear_offset < (0xFFFFul - com_segbase))
                        segrel = 0; /* user prefers flat .COM memory model, where possible */

                    sd->segment_base = com_segbase;
                    sd->segment_relative = segrel - (com_segbase >> 4ul);
                    sd->segment_offset = com_segbase + sd->linear_offset - (segrel << 4ul);

                    if (sd->segment_offset >= 0xFFFFul) {
                        dump_link_segments();
                        fprintf(stderr,"EXE: segment offset out of range\n");
                        return -1;
                    }
                }
            }
            else if (output_format == OFMT_COM) {
                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    sd->segment_relative = 0;
                    sd->segment_base = com_segbase;
                    sd->segment_offset = com_segbase + sd->linear_offset;

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

                if (output_format == OFMT_EXE) {
                    /* TODO: EXE header */
                }

                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    ofs = sd->linear_offset + file_baseofs;

                    sd->file_offset = ofs;

                    /* NTS: for EXE files, sd->file_offset will be adjusted further downward for relocation tables.
                     *      in fact, maybe computing file offset at this phase was a bad idea... :( */
                }
            }

            /* allocate in-memory copy of the segments */
            {
                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    assert(sd->image_ptr == NULL);
                    if (sd->segment_length != 0) {
                        sd->image_ptr = malloc(sd->segment_length);
                        assert(sd->image_ptr != NULL);
                        memset(sd->image_ptr,0,sd->segment_length);
                    }

                    /* reset load base */
                    sd->segment_len_count = 0;
                    sd->fragments_read = 0;
                    sd->load_base = 0;
                }
            }
        }
    }

    if (output_format == OFMT_COM && output_format_variant == OFMTVAR_COMREL) {
        /* make a new segment attached to the end, containing the relocation
         * table and the patch up code, which becomes the new entry point. */
        struct link_segdef *sg;
        struct link_symbol *sym;
        unsigned long img_size = 0;

        for (inf=0;inf < link_segments_count;inf++) {
            struct link_segdef *sd = &link_segments[inf];
            unsigned long ofs = sd->linear_offset + sd->segment_length;
            if (img_size < ofs) img_size = ofs;   
        }

        if (verbose)
            fprintf(stderr,".COM image without rel is 0x%lx bytes\n",img_size);
 
        sg = find_link_segment("__COMREL_RELOC");
        if (sg != NULL) {
            fprintf(stderr,"OBJ files are not allowed to override the COMREL relocation segment\n");
            return 1;
        }
        sg = new_link_segment("__COMREL_RELOC");
        if (sg == NULL) {
            fprintf(stderr,"Cannot allocate COMREL relocation segment\n");
            return 1;
        }

        sg->linear_offset = img_size;
        sg->segment_base = com_segbase;
        sg->segment_offset = com_segbase + sg->linear_offset;
        sg->file_offset = img_size;

        /* first, the relocation table */
        if (exe_relocation_table_count > 0) {
            unsigned long old_init_ip,init_ip;
            unsigned long ro,po;

            assert(sg->segment_length == 0);
            assert(exe_relocation_table != NULL);

            ro = sg->segment_length;
            sg->segment_length += exe_relocation_table_count * 2;

            sym = find_link_symbol("__COMREL_RELOC_TABLE");
            if (sym != NULL) return 1;
            sym = new_link_symbol("__COMREL_RELOC_TABLE");
            sym->groupdef = strdup("DGROUP");
            sym->segdef = strdup("__COMREL_RELOC");
            sym->offset = ro;

            po = sg->segment_length;
            sg->segment_length += sizeof(comrel_entry_point);

            sym = find_link_symbol("__COMREL_RELOC_ENTRY");
            if (sym != NULL) return 1;
            sym = new_link_symbol("__COMREL_RELOC_ENTRY");
            sym->groupdef = strdup("DGROUP");
            sym->segdef = strdup("__COMREL_RELOC");
            sym->offset = po;

            /* do it */
            assert(sg->image_ptr == NULL);
            sg->image_ptr = malloc(sg->segment_length);
            if (sg->image_ptr == NULL) {
                fprintf(stderr,"Unable to alloc segment\n");
                return 1;
            }

            {
                uint16_t *d = (uint16_t*)(sg->image_ptr + ro);
                uint16_t *f = (uint16_t*)(sg->image_ptr + sg->segment_length);
                struct exe_relocation *rel = exe_relocation_table;
                struct seg_fragment *frag;
                struct link_segdef *lsg;
                unsigned long roff;

                assert((d+exe_relocation_table_count) <= f);
                for (inf=0;inf < exe_relocation_table_count;inf++,rel++) {
                    assert(rel->segname != NULL);
                    lsg = find_link_segment(rel->segname);
                    if (lsg == NULL) {
                        fprintf(stderr,"COM relocation entry refers to non-existent segment '%s'\n",rel->segname);
                        return 1;
                    }

                    assert(lsg->fragments != NULL);
                    assert(lsg->fragments_count != 0);
                    assert(lsg->fragments_count <= lsg->fragments_alloc);
                    assert(rel->fragment < lsg->fragments_count);
                    frag = lsg->fragments + rel->fragment;

                    roff = rel->offset + lsg->linear_offset + frag->offset + com_segbase;

                    if (roff >= (0xFF00u - (exe_relocation_table_count * 2u))) {
                        fprintf(stderr,"COM relocation entry is non-representable\n");
                        return 1;
                    }

                    d[inf] = (uint16_t)roff;
                }
            }

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

            {
                uint8_t *d = (uint8_t*)(sg->image_ptr + po);
                uint8_t *f = (uint8_t*)(sg->image_ptr + sg->segment_length);

                assert((d+sizeof(comrel_entry_point)) <= f);
                memcpy(d,comrel_entry_point,sizeof(comrel_entry_point));

                *((uint16_t*)(d+comrel_entry_point_CX_COUNT)) = exe_relocation_table_count;
                *((uint16_t*)(d+comrel_entry_point_SI_OFFSET)) = ro + sg->segment_offset;
                *((uint16_t*)(d+comrel_entry_point_JMP_ENTRY)) = old_init_ip - (init_ip + comrel_entry_point_JMP_ENTRY + 2);
            }

            /* change entry point to new entry point */
            if (verbose) {
                fprintf(stderr,"Old entry IP=0x%lx\n",old_init_ip);
                fprintf(stderr,"New entry IP=0x%lx\n",init_ip);
            }

            cstr_free(&entry_seg_link_target_name);
            entry_seg_link_target_name = strdup(sg->name);
            entry_seg_link_target = sg;
            entry_seg_ofs = po;
        }
    }

    if (verbose) {
        dump_link_relocations();
        dump_link_symbols();
        dump_link_segments();
    }

    /* write output */
    assert(out_file != NULL);
    {
        int fd;

        fd = open(out_file,O_RDWR|O_BINARY|O_CREAT|O_TRUNC,0644);
        if (fd < 0) {
            fprintf(stderr,"Unable to open output file\n");
            return 1;
        }

        if (output_format == OFMT_EXE) {
            /* EXE header */
            unsigned char tmp[32];
            unsigned long disk_size = 0;
            unsigned long header_size = 0;
            unsigned long o_header_size = 0;
            unsigned long resident_size = 0;
            unsigned long relocation_table_offset = 0;
            unsigned long max_resident_size = 0xFFFF0ul; /* by default, take ALL memory */
            unsigned long init_ss = 0,init_sp = 0,init_cs = 0,init_ip = 0;
            unsigned int stack_size = 4096;/*reasonable default*/
            unsigned int i,ofs;

            if (link_segments_count > 0) {
                struct link_segdef *sd = &link_segments[0];
                header_size = sd->file_offset;
            }
            o_header_size = header_size;

            if (exe_relocation_table_count != 0) {
                assert(header_size >= 0x20);
                relocation_table_offset = header_size;
                header_size += (exe_relocation_table_count * 4ul);
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

                ofs = sd->file_offset + sd->segment_length;

                /* NTS: "disk size" includes EXE header, everything needed for MS-DOS
                 *      to parse and load the EXE file */

                /* TODO: Some segments, like BSS or STACK, do not get emitted to disk.
                 *       Except that in the EXE format, segments can only be omitted
                 *       from the end of the image. */

                if (disk_size < ofs) disk_size = ofs;
                if (resident_size < ofs) resident_size = ofs;
            }

            /* TODO: Use the size and position of the STACK segment! */
            /* Take additional resident size for the stack */
            resident_size += stack_size;
            ofs = resident_size;
            assert(resident_size >= 32);
            init_ss = 0;
            init_sp = resident_size - 16; /* assume MS-DOS will consume some stack on entry to program */
            while (init_sp >= 0xFFF0u) {
                init_ss++;
                init_sp -= 0x10;
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

                if (verbose)
                    fprintf(stderr,"EXE entry: %04lx:%04lx in %s\n",init_cs,init_ip,entry_seg_link_target->name);
            }
            else {
                fprintf(stderr,"EXE warning: No entry point\n");
            }

            if (verbose)
                fprintf(stderr,"EXE header size: %lu\n",header_size);

            assert(resident_size > disk_size);
            assert(header_size != 0u);
            assert((header_size % 16u) == 0u);
            assert(sizeof(tmp) >= 32);
            memset(tmp,0,32);
            tmp[0] = 'M';
            tmp[1] = 'Z';

            if (verbose) {
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

                if (exe_relocation_table_count != 0) {
                    *((uint16_t*)(tmp+24)) = (uint16_t)relocation_table_offset;
                    *((uint16_t*)(tmp+6)) = (uint16_t)exe_relocation_table_count;
                }
            }

            if (lseek(fd,0,SEEK_SET) == 0) {
                if (write(fd,tmp,32) == 32) {
                    /* good */
                }
            }

            if (exe_relocation_table_count != 0) {
                if ((unsigned long)lseek(fd,relocation_table_offset,SEEK_SET) == relocation_table_offset) {
                    struct exe_relocation *rel = exe_relocation_table;
                    struct seg_fragment *frag;
                    struct link_segdef *lsg;
                    unsigned long rseg,roff;

                    for (inf=0;inf < exe_relocation_table_count;inf++,rel++) {
                        assert(rel->segname != NULL);
                        lsg = find_link_segment(rel->segname);
                        if (lsg == NULL) {
                            fprintf(stderr,"COM relocation entry refers to non-existent segment '%s'\n",rel->segname);
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
        else if (output_format == OFMT_COM) {
            /* .COM require JMP instruction */
            if (entry_seg_link_target != NULL && com_entry_insert > 0) {
                unsigned long ofs = (entry_seg_link_target->linear_offset+entry_seg_ofs);
                unsigned char tmp[4];

                assert(com_entry_insert < 4);

                /* TODO: Some segments, like BSS or STACK, do not get emitted to disk.
                 *       Except that in the COM format, segments can only be omitted
                 *       from the end of the image. */

                if (ofs >= (unsigned long)com_entry_insert) {
                    ofs -= (unsigned long)com_entry_insert;

                    if (lseek(fd,0,SEEK_SET) == 0) {
                        if (com_entry_insert == 3) {
                            tmp[0] = 0xE9; /* JMP near */
                            *((uint16_t*)(tmp+1)) = (uint16_t)ofs;
                            if (write(fd,tmp,3) == 3) {
                                /* good */
                            }
                        }
                        else if (com_entry_insert == 2) {
                            tmp[0] = 0xEB; /* JMP short */
                            tmp[1] = (unsigned char)ofs;
                            if (write(fd,tmp,2) == 2) {
                                /* good */
                            }
                        }
                        else {
                            abort();
                        }
                    }
                }
            }
        }
        else {
            abort();
        }

        for (inf=0;inf < link_segments_count;inf++) {
            struct link_segdef *sd = &link_segments[inf];

            if (sd->segment_length == 0) continue;

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

        close(fd);
    }

    cstr_free(&entry_seg_link_target_name);
    cstr_free(&entry_seg_link_frame_name);

    link_symbols_free();
    free_link_segments();
    free_exe_relocations();
    return 0;
}

