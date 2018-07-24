/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

//================================== PROGRAM ================================

#define MAX_SYMBOLS                     65536

struct link_symbol {
    char*                               name;
    char*                               segdef;
    char*                               groupdef;
    unsigned long                       offset;
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
};

static struct link_segdef               link_segments[MAX_SEGMENTS];
static unsigned int                     link_segments_count = 0;

static struct link_segdef*              current_link_segment = NULL;

static struct link_segdef*              entry_seg_link_target = NULL;
static struct link_segdef*              entry_seg_link_frame = NULL;
static unsigned char                    com_entry_insert = 0;
static unsigned long                    entry_seg_ofs = 0;

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

void link_segments_swap(unsigned int s1,unsigned int s2) {
    if (s1 != s2) {
        struct link_segdef t;

                        t = link_segments[s1];
        link_segments[s1] = link_segments[s2];
        link_segments[s2] = t;
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
        else {
            i++;
        }
    }
}

void owlink_dosseg_sort_order(void) {
    unsigned int s = 0,e = link_segments_count-1;

    if (link_segments_count == 0) return;
    link_segments_sort(&s,&e,sort_cmp_not_dgroup_class_code);       /* 1 */
    link_segments_sort(&s,&e,sort_cmp_not_dgroup);                  /* 2 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_BEGDATA);        /* 3 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_not_special);    /* 4 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_bss);            /* 5 */
    link_segments_sort(&s,&e,sort_cmp_dgroup_class_stack);          /* 6 */
}

void free_link_segment(struct link_segdef *sg) {
    cstr_free(&(sg->groupname));
    cstr_free(&(sg->classname));
    cstr_free(&(sg->name));

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

void dump_link_symbols(void) {
    unsigned int i=0;

    if (link_symbols == NULL) return;

    while (i < link_symbols_count) {
        struct link_symbol *sym = &link_symbols[i++];
        if (sym->name == NULL) continue;

        fprintf(stderr,"symbol[%u]: name='%s' group='%s' seg='%s' offset=0x%lx\n",
            i/*post-increment, intentional*/,sym->name,sym->groupdef,sym->segdef,sym->offset);
    }
}

void dump_link_segments(void) {
    unsigned int i=0;

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

int ledata_add(struct omf_context_t *omf_state, struct omf_ledata_info_t *info) {
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

    max_ofs = (unsigned long)info->enum_data_offset + (unsigned long)info->data_length;
    if (lsg->segment_len_count < max_ofs) lsg->segment_len_count = max_ofs;
    max_ofs += (unsigned long)lsg->load_base;
    if (lsg->segment_length < max_ofs) {
        fprintf(stderr,"LEDATA out of bounds (len=%lu max=%lu)\n",lsg->segment_length,max_ofs);
        return 1;
    }

    assert(info->data != NULL);
    assert(lsg->image_ptr != NULL);
    assert(max_ofs >= (unsigned long)info->data_length);
    max_ofs -= (unsigned long)info->data_length;
    memcpy(lsg->image_ptr + max_ofs, info->data, info->data_length);

    return 0;
}

int ledata_note(struct omf_context_t *omf_state, struct omf_ledata_info_t *info) {
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

    max_ofs = (unsigned long)info->enum_data_offset + (unsigned long)info->data_length;
    if (lsg->segment_len_count < max_ofs) lsg->segment_len_count = max_ofs;
    max_ofs += (unsigned long)lsg->load_base;
    if (lsg->segment_length < max_ofs) lsg->segment_length = max_ofs;

    return 0;
}

int fixupp_get(struct omf_context_t *omf_state,unsigned long *fseg,unsigned long *fofs,const struct omf_fixupp_t *ent,unsigned int method,unsigned int index) {
    *fseg = *fofs = ~0UL;
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
            fprintf(stderr,"FIXUPP SEGDEF not found\n");
            return -1;
        }

        *fseg = lsg->segment_relative;
        *fofs = lsg->segment_offset;
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
    }
    else if (method == 2/*EXTDEF*/) {
        fprintf(stderr,"FRAME EXTDEF not impl\n");
    }
    else if (method == 5/*BY TARGET*/) {
    }
    else {
        fprintf(stderr,"FRAME UNSUPP not impl\n");
    }

    return 0;
}

int apply_FIXUPP(struct omf_context_t *omf_state,unsigned int first) {
    unsigned long final_seg,final_ofs;
    unsigned long frame_seg,frame_ofs;
    unsigned long targ_seg,targ_ofs;
    unsigned char *fence;
    unsigned char *ptr;
    unsigned long ptch;

    assert(current_link_segment != NULL);
    assert(current_link_segment->image_ptr != NULL);
    fence = current_link_segment->image_ptr + current_link_segment->segment_length;

    while (first <= omf_fixupps_context_get_highest_index(&omf_state->FIXUPPs)) {
        const struct omf_fixupp_t *ent = omf_fixupps_context_get_fixupp(&omf_state->FIXUPPs,first++);
        if (ent == NULL) continue;
        if (!ent->alloc) continue;

        if (!ent->segment_relative) {
            fprintf(stderr,"Self-relative not yet supported\n");
            continue;
        }

        if (fixupp_get(omf_state,&frame_seg,&frame_ofs,ent,ent->frame_method,ent->frame_index))
            return -1;
        if (fixupp_get(omf_state,&targ_seg,&targ_ofs,ent,ent->target_method,ent->target_index))
            return -1;

        if (ent->frame_method == 5/*BY TARGET*/) {
            frame_seg = targ_seg;
            frame_ofs = targ_ofs;
        }

        if (omf_state->flags.verbose) {
            fprintf(stderr,"fixup[%u] frame=%lx:%lx targ=%lx:%lx\n",
                    first,
                    frame_seg,frame_ofs,
                    targ_seg,targ_ofs);
        }

        if (frame_seg == ~0UL || frame_ofs == ~0UL) {
            fprintf(stderr,"frame addr not resolved\n");
            continue;
        }
        if (targ_seg == ~0UL || targ_ofs == ~0UL) {
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

        ptch = (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset;

        if (omf_state->flags.verbose)
            fprintf(stderr,"ptch=0x%lx\n",ptch);

        ptr = current_link_segment->image_ptr + ptch;
        assert(ptr < fence);

        switch (ent->location) {
            case OMF_FIXUPP_LOCATION_16BIT_OFFSET: /* 16-bit offset */
                assert((ptr+2) <= fence);
                *((uint16_t*)ptr) += (uint16_t)final_ofs;
                break;
            default:
                fprintf(stderr,"Unsupported\n");
                break;
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

int pubdef_add(struct omf_context_t *omf_state,unsigned int first,unsigned int tag) {
    const unsigned char is_local = (tag == OMF_RECTYPE_LPUBDEF) || (tag == OMF_RECTYPE_LPUBDEF32);

    // TODO
    (void)is_local;

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

        fprintf(stderr,"pubdef[%u]: '%s' group='%s' seg='%s' offset=0x%lx\n",
            first,name,groupname,segname,(unsigned long)pubdef->public_offset);

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
        sym->offset = pubdef->public_offset + lsg->load_base;
        sym->groupdef = strdup(groupname);
        sym->segdef = strdup(segname);
    }

    return 0;
}

int segdef_add(struct omf_context_t *omf_state,unsigned int first) {
    unsigned long alignb,malign;
    struct link_segdef *lsg;

    while (first < omf_state->SEGDEFs.omf_SEGDEFS_count) {
        struct omf_segdef_t *sg = &omf_state->SEGDEFs.omf_SEGDEFS[first++];
        const char *classname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->class_name_index);
        const char *name = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,sg->segment_name_index);

        if (*name == 0) continue;

        lsg = find_link_segment(name);
        if (lsg != NULL) {
            /* it is an error to change attributes */
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

        fprintf(stderr,"Start segment='%s' load=0x%lx\n",
            lsg->name, lsg->load_base);
    }

    return 0;
}

#define MAX_GROUPS                      256

#define MAX_IN_FILES                    256

static char*                            out_file = NULL;

static char*                            in_file[MAX_IN_FILES];
static unsigned int                     in_file_count = 0;
static unsigned int                     current_in_file = 0;
static unsigned int                     current_in_mod = 0;

static unsigned char                    do_dosseg = 1;

static unsigned short                   com_segbase = 0; /* Watcom Linker behavior: .COM starts at 0, then
                                                            entry point has to fiddle with instruction pointer
                                                            and sregs to adjust */

struct omf_context_t*                   omf_state = NULL;

static void help(void) {
    fprintf(stderr,"lnkdos16 [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to link\n");
    fprintf(stderr,"  -o <file>    Output file\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
    fprintf(stderr,"  -no-dosseg   No DOSSEG sort order\n");
    fprintf(stderr,"  -dosseg      DOSSEG sort order\n");
    fprintf(stderr,"  -com100      Link .COM segment starting at 0x100\n");
    fprintf(stderr,"  -com0        Link .COM segment starting at 0 (Watcom Linker)\n");
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

    printf("----END-----\n");
}

int main(int argc,char **argv) {
    unsigned char verbose = 0;
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
            else if (!strcmp(a,"com0")) {
                com_segbase = 0x000;
            }
            else if (!strcmp(a,"com100")) {
                com_segbase = 0x100;
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

    if (in_file_count == 0) {
        help();
        return 1;
    }

    if (out_file == NULL) {
        help();
        return 1;
    }

    for (pass=0;pass < 2;pass++) {
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
                    if (omf_record_is_modend(&omf_state->record)) {
                        if (!diddump) {
                            my_dumpstate(omf_state);
                            diddump = 1;
                        }

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

                            if (pass == 0 && pubdef_add(omf_state, p_count, omf_state->record.rectype))
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

                            if (pass == 0 && segdef_add(omf_state, p_count))
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

                            if (pass == 0 && grpdef_add(omf_state, p_count))
                                return 1;
                        } break;
                    case OMF_RECTYPE_FIXUPP:/*0x9C*/
                    case OMF_RECTYPE_FIXUPP32:/*0x9D*/
                        {
                            int p_count = omf_state->FIXUPPs.omf_FIXUPPS_count;
                            int first_new_fixupp;

                            if ((first_new_fixupp=omf_context_parse_FIXUPP(omf_state,&omf_state->record)) < 0) {
                                fprintf(stderr,"Error parsing FIXUPP\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose)
                                dump_FIXUPP(stdout,omf_state,(unsigned int)first_new_fixupp);

                            if (pass == 1 && apply_FIXUPP(omf_state,p_count))
                                return 1;
                        } break;
                    case OMF_RECTYPE_LEDATA:/*0xA0*/
                    case OMF_RECTYPE_LEDATA32:/*0xA1*/
                        {
                            struct omf_ledata_info_t info;

                            if (omf_context_parse_LEDATA(omf_state,&info,&omf_state->record) < 0) {
                                fprintf(stderr,"Error parsing LEDATA\n");
                                return 1;
                            }

                            if (omf_state->flags.verbose && pass == 0)
                                dump_LEDATA(stdout,omf_state,&info);

                            if (pass == 0 && ledata_note(omf_state, &info))
                                return 1;
                            if (pass == 1 && ledata_add(omf_state, &info))
                                return 1;
                        } break;
                    case OMF_RECTYPE_MODEND:/*0x8A*/
                    case OMF_RECTYPE_MODEND32:/*0x8B*/
                        if (pass == 0) {
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

                                if (frame_segdef != NULL && target_segdef != NULL) {
                                    const char *framename = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,frame_segdef->segment_name_index);
                                    const char *targetname = omf_lnames_context_get_name_safe(&omf_state->LNAMEs,target_segdef->segment_name_index);

                                    fprintf(stderr,"'%s' vs '%s'\n",framename,targetname);
                                    if (*framename != 0 && *targetname != 0) {
                                        struct link_segdef *frameseg,*targseg;

                                        targseg = find_link_segment(targetname);
                                        frameseg = find_link_segment(framename);
                                        if (targseg != NULL && frameseg != NULL) {
                                            entry_seg_ofs = TargetDisplacement;
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

            if (!diddump) {
                my_dumpstate(omf_state);
                diddump = 1;
            }

            omf_context_clear(omf_state);
            omf_state = omf_context_destroy(omf_state);

            close(fd);
        }

        if (pass == 0) {
            if (do_dosseg)
                owlink_dosseg_sort_order();

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
                if (0) {
                    /* EXE */
                }
                else {
                    /* COM */
                    if (entry_seg_link_target != NULL) {
                        unsigned long io = (entry_seg_link_target->linear_offset+entry_seg_ofs);

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

                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    if (sd->initial_alignment != 0ul) {
                        m = ofs % sd->initial_alignment;
                        if (m != 0ul) ofs += sd->initial_alignment - m;
                    }

                    fprintf(stderr,"segment[%u] ofs=0x%lx len=0x%lx\n",
                        inf,ofs,sd->segment_length);

                    sd->linear_offset = ofs;
                    ofs += sd->segment_length;
                }
            }

            /* if a .COM executable, then all segments are arranged so that the first byte
             * is at 0x100 */
            if (0) {
                /* EXE */
            }
            else {
                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    sd->segment_base = com_segbase;
                    sd->segment_offset = com_segbase + sd->linear_offset;
                }
            }

            /* decide where the segments end up in the executable */
            {
                unsigned long ofs = 0;

                if (0) {
                    /* TODO: EXE header */
                }

                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    ofs = sd->linear_offset;
                    /* TODO: adjust by EXE header size */

                    sd->file_offset = ofs;
                }
            }

            /* allocate in-memory copy of the segments */
            {
                for (inf=0;inf < link_segments_count;inf++) {
                    struct link_segdef *sd = &link_segments[inf];

                    assert(sd->image_ptr == NULL);
                    if (sd->segment_length == 0) continue;

                    sd->image_ptr = malloc(sd->segment_length);
                    assert(sd->image_ptr != NULL);
                    memset(sd->image_ptr,0,sd->segment_length);

                    /* reset load base */
                    sd->segment_len_count = 0;
                    sd->load_base = 0;
                }
            }
        }
    }

    if (verbose) {
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

        if (0) {
        }
        else {
            /* .COM require JMP instruction */
            if (entry_seg_link_target != NULL && com_entry_insert > 0) {
                unsigned long ofs = (entry_seg_link_target->linear_offset+entry_seg_ofs);
                unsigned char tmp[4];

                assert(com_entry_insert < 4);

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

    link_symbols_free();
    free_link_segments();
    return 0;
}

