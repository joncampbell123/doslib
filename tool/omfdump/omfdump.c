/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "omfcstr.h"
#include "omfrec.h"

#ifndef O_BINARY
#define O_BINARY (0)
#endif

//================================== OMF rec types strings ================================

const char *omf_rectype_to_str_long(unsigned char rt) {
    switch (rt) {
        case 0x80:  return "Translator Header Record";
        case 0x82:  return "Library Module Header Record";
        case 0x88:  return "Comment Record";
        case 0x8A:  return "Module End Record";
        case 0x8B:  return "Module End Record (32-bit)";
        case 0x8C:  return "External Names Definition Record";
        case 0x90:  return "Public Names Definition Record";
        case 0x91:  return "Public Names Definition Record (32-bit)";
        case 0x94:  return "Line Numbers Record";
        case 0x95:  return "Line Numbers Record (32-bit)";
        case 0x96:  return "List of Names Record";
        case 0x98:  return "Segment Definition Record";
        case 0x99:  return "Segment Definition Record (32-bit)";
        case 0x9A:  return "Group Definition Record";
        case 0x9C:  return "Fixup Record";
        case 0x9D:  return "Fixup Record (32-bit)";
        case 0xA0:  return "Logical Enumerated Data Record";
        case 0xA1:  return "Logical Enumerated Data Record (32-bit)";
        case 0xA2:  return "Logical Iterated Data Record";
        case 0xA3:  return "Logical Iterated Data Record (32-bit)";
        case 0xB0:  return "Communal Names Definition Record";
        case 0xB2:  return "Backpatch Record";
        case 0xB3:  return "Backpatch Record (32-bit)";
        case 0xB4:  return "Local External Names Definition Record";
        case 0xB6:  return "Local Public Names Definition Record";
        case 0xB7:  return "Local Public Names Definition Record (32-bit)";
        case 0xB8:  return "Local Communal Names Definition Record";
        case 0xBC:  return "COMDAT External Names Definition Record";
        case 0xC2:  return "Initialized Communal Data Record";
        case 0xC3:  return "Initialized Communal Data Record (32-bit)";
        case 0xC4:  return "Symbol Line Numbers Record";
        case 0xC5:  return "Symbol Line Numbers Record (32-bit)";
        case 0xC6:  return "Alias Definition Record";
        case 0xC8:  return "Named Backpatch Record";
        case 0xC9:  return "Named Backpatch Record (32-bit)";
        case 0xCA:  return "Local Logical Names Definition Record";
        case 0xCC:  return "OMF Version Number Record";
        case 0xCE:  return "Vendor-specific OMF Extension Record";
        case 0xF0:  return "Library Header Record";
        case 0xF1:  return "Library End Record";
        default:    break;
    }

    return "?";
}

const char *omf_rectype_to_str(unsigned char rt) {
    switch (rt) {
        case 0x80:  return "THEADR";
        case 0x82:  return "LHEADR";
        case 0x88:  return "COMENT";
        case 0x8A:  return "MODEND";
        case 0x8B:  return "MODEND32";
        case 0x8C:  return "EXTDEF";
        case 0x90:  return "PUBDEF";
        case 0x91:  return "PUBDEF32";
        case 0x94:  return "LINNUM";
        case 0x95:  return "LINNUM32";
        case 0x96:  return "LNAMES";
        case 0x98:  return "SEGDEF";
        case 0x99:  return "SEGDEF32";
        case 0x9A:  return "GRPDEF";
        case 0x9C:  return "FIXUPP";
        case 0x9D:  return "FIXUPP32";
        case 0xA0:  return "LEDATA";
        case 0xA1:  return "LEDATA32";
        case 0xA2:  return "LIDATA";
        case 0xA3:  return "LIDATA32";
        case 0xB0:  return "COMDEF";
        case 0xB2:  return "BAKPAT";
        case 0xB3:  return "BAKPAT32";
        case 0xB4:  return "LEXTDEF";
        case 0xB6:  return "LPUBDEF";
        case 0xB7:  return "LPUBDEF32";
        case 0xB8:  return "LCOMDEF";
        case 0xBC:  return "CEXTDEF";
        case 0xC2:  return "COMDAT";
        case 0xC3:  return "COMDAT32";
        case 0xC4:  return "LINSYM";
        case 0xC5:  return "LINSYM32";
        case 0xC6:  return "ALIAS";
        case 0xC8:  return "NBKPAT";
        case 0xC9:  return "NBKPAT32";
        case 0xCA:  return "LLNAMES";
        case 0xCC:  return "VERNUM";
        case 0xCE:  return "VENDEXT";
        case 0xF0:  return "LIBHEAD"; /* I made up this name */
        case 0xF1:  return "LIBEND"; /* I also made up this name */
        default:    break;
    }

    return "?";
}

//================================== LNAMES ================================

/* LNAMES collection */
struct omf_lnames_context_t {
    char**              omf_LNAMES;
    unsigned int        omf_LNAMES_count;
    unsigned int        omf_LNAMES_alloc;
};

void omf_lnames_context_init(struct omf_lnames_context_t * const ctx) {
    ctx->omf_LNAMES = NULL;
    ctx->omf_LNAMES_count = 0;
#if defined(LINUX)
    ctx->omf_LNAMES_alloc = 32768;
#else
    ctx->omf_LNAMES_alloc = 1024;
#endif
}

int omf_lnames_context_alloc_names(struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES != NULL)
        return 0;

    if (ctx->omf_LNAMES_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_LNAMES_count = 0;
    ctx->omf_LNAMES = (char**)malloc(sizeof(char*) * ctx->omf_LNAMES_alloc);
    if (ctx->omf_LNAMES == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

int omf_lnames_context_clear_name(struct omf_lnames_context_t * const ctx,unsigned int i) {
    if ((i--) == 0) {
        errno = ERANGE;
        return -1; /* LNAMEs are indexed 1-based. After this test, i is converted to 0-based index */
    }

    if (i >= ctx->omf_LNAMES_count) {
        errno = ERANGE;
        return -1; /* out of range */
    }

    if (ctx->omf_LNAMES == NULL)
        return 0; /* LNAMEs array not allocated */

    if (ctx->omf_LNAMES[i] != NULL) {
        free(ctx->omf_LNAMES[i]);
        ctx->omf_LNAMES[i] = NULL;
    }

    return 0;
}

const char *omf_lnames_context_get_name(const struct omf_lnames_context_t * const ctx,unsigned int i) {
    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_LNAMES_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_LNAMES[i];
}

const char *omf_lnames_context_get_name_safe(const struct omf_lnames_context_t * const ctx,unsigned int i) {
    const char *r = omf_lnames_context_get_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

int omf_lnames_context_set_name(struct omf_lnames_context_t * const ctx,unsigned int i,const char *name,const unsigned char namelen) {
    if (name == NULL) {
        errno = EFAULT;
        return -1;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return -1; /* LNAMEs are indexed 1-based. After this test, i is converted to 0-based index */
    }

    if (i >= ctx->omf_LNAMES_alloc) {
        errno = ERANGE;
        return -1;
    }

    if (ctx->omf_LNAMES == NULL) {
        if (omf_lnames_context_alloc_names(ctx) < 0)
            return -1; /* sets errno */
    }

    while (ctx->omf_LNAMES_count <= i)
        ctx->omf_LNAMES[ctx->omf_LNAMES_count++] = NULL;

    if (cstr_set_n(&ctx->omf_LNAMES[i],name,namelen) < 0)
        return -1;

    return 0;
}

int omf_lnames_context_add_name(struct omf_lnames_context_t * const ctx,const char *name,const unsigned char namelen) {
    unsigned int idx;

    if (ctx->omf_LNAMES != NULL)
        idx = ctx->omf_LNAMES_count + 1;
    else
        idx = 1;

    if (omf_lnames_context_set_name(ctx,idx,name,namelen) < 0)
        return -1; /* sets errno */

    return (int)idx;
}

void omf_lnames_context_clear_names(struct omf_lnames_context_t * const ctx) {
    char *p;

    while (ctx->omf_LNAMES_count > 0) {
        --ctx->omf_LNAMES_count;
        p = ctx->omf_LNAMES[ctx->omf_LNAMES_count];
        ctx->omf_LNAMES[ctx->omf_LNAMES_count] = NULL;
        if (p != NULL) free(p);
    }
}

void omf_lnames_context_free_names(struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES) {
        omf_lnames_context_clear_names(ctx);
        free(ctx->omf_LNAMES);
        ctx->omf_LNAMES = NULL;
    }
    ctx->omf_LNAMES_count = 0;
}

void omf_lnames_context_free(struct omf_lnames_context_t * const ctx) {
    omf_lnames_context_free_names(ctx);
}

struct omf_lnames_context_t *omf_lnames_context_create(void) {
    struct omf_lnames_context_t *ctx;

    ctx = (struct omf_lnames_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_lnames_context_init(ctx);
    return ctx;
}

struct omf_lnames_context_t *omf_lnames_context_destroy(struct omf_lnames_context_t * const ctx) {
    if (ctx != NULL) {
        omf_lnames_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

// return the lowest valid LNAME index
static inline unsigned int omf_lnames_context_get_lowest_index(const struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES == NULL)
        return 0;

    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_lnames_context_get_highest_index(const struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES == NULL)
        return 0;

    return ctx->omf_LNAMES_count;
}

// return the index the next LNAME will get after calling add_name()
static inline unsigned int omf_lnames_context_get_next_add_index(const struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES == NULL)
        return 0;

    return ctx->omf_LNAMES_count + 1;
}

//================================== SEGDEFS ================================

/* SEGDEFS collection */
#pragma pack(push,1)
struct omf_segdef_attr_t {
    union {
        unsigned char       raw;
        struct {
            // NTS: This assumes GCC x86 bitfield order where the LSB comes first
            unsigned int    use32:1;        // [0:0] aka "P"
            unsigned int    big_segment:1;  // [1:1] length == 0, 16-bit segment is 64KB, 32-bit segment is 4GB
            unsigned int    combination:3;  // [4:2]
            unsigned int    alignment:3;    // [7:5]
        } f;
    } f;
    uint16_t            frame_number;   // <conditional> if alignment == 0 (absolute)
    uint8_t             offset;         // <conditional> if alignment == 0 (absolute)
};

struct omf_segdef_t {
    struct omf_segdef_attr_t        attr;
    uint32_t                        segment_length;
    uint16_t                        segment_name_index;
    uint16_t                        class_name_index;
    uint16_t                        overlay_name_index;
};
#pragma pack(pop)

struct omf_segdefs_context_t {
    struct omf_segdef_t*            omf_SEGDEFS;
    unsigned int                    omf_SEGDEFS_count;
    unsigned int                    omf_SEGDEFS_alloc;
};

void omf_segdefs_context_init_segdef(struct omf_segdef_t *s) {
    memset(s,0,sizeof(*s));
}

void omf_segdefs_context_init(struct omf_segdefs_context_t * const ctx) {
    ctx->omf_SEGDEFS = NULL;
    ctx->omf_SEGDEFS_count = 0;
#if defined(LINUX)
    ctx->omf_SEGDEFS_alloc = 32768;
#else
    ctx->omf_SEGDEFS_alloc = 256;
#endif
}

int omf_segdefs_context_alloc_segdefs(struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS != NULL)
        return 0;

    if (ctx->omf_SEGDEFS_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_SEGDEFS_count = 0;
    ctx->omf_SEGDEFS = (struct omf_segdef_t*)malloc(sizeof(struct omf_segdef_t) * ctx->omf_SEGDEFS_alloc);
    if (ctx->omf_SEGDEFS == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

void omf_segdefs_context_free_entries(struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS) {
        free(ctx->omf_SEGDEFS);
        ctx->omf_SEGDEFS = NULL;
    }
    ctx->omf_SEGDEFS_count = 0;
}

void omf_segdefs_context_free(struct omf_segdefs_context_t * const ctx) {
    omf_segdefs_context_free_entries(ctx);
}

struct omf_segdefs_context_t *omf_segdefs_context_create(void) {
    struct omf_segdefs_context_t *ctx;

    ctx = (struct omf_segdefs_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_segdefs_context_init(ctx);
    return ctx;
}

struct omf_segdefs_context_t *omf_segdefs_context_destroy(struct omf_segdefs_context_t * const ctx) {
    if (ctx != NULL) {
        omf_segdefs_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

// return the lowest valid LNAME index
static inline unsigned int omf_segdefs_context_get_lowest_index(const struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS == NULL)
        return 0;

    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_segdefs_context_get_highest_index(const struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS == NULL)
        return 0;

    return ctx->omf_SEGDEFS_count;
}

// return the index the next LNAME will get after calling add_segdef()
static inline unsigned int omf_segdefs_context_get_next_add_index(const struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS == NULL)
        return 0;

    return ctx->omf_SEGDEFS_count + 1;
}

struct omf_segdef_t *omf_segdefs_context_add_segdef(struct omf_segdefs_context_t * const ctx) {
    struct omf_segdef_t *seg;

    if (omf_segdefs_context_alloc_segdefs(ctx) < 0)
        return NULL;

    if (ctx->omf_SEGDEFS_count >= ctx->omf_SEGDEFS_alloc) {
        errno = ERANGE;
        return NULL;
    }

    seg = ctx->omf_SEGDEFS + ctx->omf_SEGDEFS_count;
    ctx->omf_SEGDEFS_count++;

    omf_segdefs_context_init_segdef(seg);
    return seg;
}

const struct omf_segdef_t *omf_segdefs_context_get_segdef(const struct omf_segdefs_context_t * const ctx,unsigned int i) {
    if (ctx->omf_SEGDEFS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_SEGDEFS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_SEGDEFS + i;
}

//================================== OMF ================================

char                                    templstr[255+1/*NUL*/];

struct omf_context_t {
    const char*                         last_error;
    struct omf_lnames_context_t         LNAMEs;
    struct omf_segdefs_context_t        SEGDEFs;
    struct omf_record_t                 record; // reading, during parsing
    unsigned short                      library_block_size;// is .LIB archive if nonzero
    char*                               THEADR;
    struct {
        unsigned int                    verbose:1;
    } flags;
};

void omf_context_init(struct omf_context_t * const ctx) {
    omf_segdefs_context_init(&ctx->SEGDEFs);
    omf_lnames_context_init(&ctx->LNAMEs);
    omf_record_init(&ctx->record);
    ctx->last_error = NULL;
    ctx->flags.verbose = 0;
    ctx->library_block_size = 0;
    ctx->THEADR = NULL;
}

void omf_context_free(struct omf_context_t * const ctx) {
    omf_segdefs_context_free(&ctx->SEGDEFs);
    omf_lnames_context_free(&ctx->LNAMEs);
    omf_record_free(&ctx->record);
    cstr_free(&ctx->THEADR);
}

struct omf_context_t *omf_context_create(void) {
    struct omf_context_t *ctx;

    ctx = malloc(sizeof(*ctx));
    if (ctx != NULL) omf_context_init(ctx);
    return ctx;
}

struct omf_context_t *omf_context_destroy(struct omf_context_t * const ctx) {
    if (ctx != NULL) {
        omf_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

void omf_context_begin_file(struct omf_context_t * const ctx) {
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_begin_module(struct omf_context_t * const ctx) {
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    cstr_free(&ctx->THEADR);
}

void omf_context_clear(struct omf_context_t * const ctx) {
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
}

int omf_context_next_lib_module_fd(struct omf_context_t * const ctx,int fd) {
    unsigned long ofs;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was not a MODEND, then stop reading.
    if ((ctx->record.rectype&0xFE) != 0x8A) { // Not 0x8A or 0x8B
        errno = EIO;
        return -1;
    }

    // if we don't have a block size, then we cannot advance
    if (ctx->library_block_size == 0UL)
        return 0;

    // where does the next block size start?
    ofs = ctx->record.rec_file_offset + 3 + ctx->record.reclen;
    ofs += ctx->library_block_size - 1UL;
    ofs -= ofs % ctx->library_block_size;
    if (lseek(fd,(off_t)ofs,SEEK_SET) != (off_t)ofs)
        return 0;

    ctx->record.rec_file_offset = ofs;
    ctx->record.rectype = 0;
    ctx->record.reclen = 0;
    return 1;
}

int omf_context_read_fd(struct omf_context_t * const ctx,int fd) {
    unsigned char sum = 0;
    unsigned char tmp[3];
    unsigned int i;
    int ret;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was a MODEND, then stop reading, make caller move to next module with another function
    if ((ctx->record.rectype&0xFE) == 0x8A) // 0x8A or 0x8B
        return 0;

    ctx->last_error = NULL;
    omf_record_clear(&ctx->record);
    if (ctx->record.data == NULL && omf_record_data_alloc(&ctx->record,0) < 0)
        return -1; // sets errno
    if (ctx->record.data_alloc < 16) {
        ctx->last_error = "Record buffer too small";
        errno = EINVAL;
        return -1;
    }

    ctx->record.rec_file_offset = lseek(fd,0,SEEK_CUR);

    if ((ret=read(fd,tmp,3)) != 3) {
        if (ret >= 0) {
            return 0; // EOF
        }

        ctx->last_error = "Reading OMF record header failed";
        // read sets errno
        return -1;
    }
    ctx->record.rectype = tmp[0];
    ctx->record.reclen = *((uint16_t*)(tmp+1)); // length (including checksum)
    if (ctx->record.rectype == 0 || ctx->record.reclen == 0)
        return 0;
    if (ctx->record.reclen > ctx->record.data_alloc) {
        ctx->last_error = "Reading OMF record failed because record too large for buffer";
        errno = ERANGE;
        return -1;
    }
    if ((unsigned int)(ret=read(fd,ctx->record.data,ctx->record.reclen)) != (unsigned int)ctx->record.reclen) {
        ctx->last_error = "Reading OMF record contents failed";
        if (ret >= 0) errno = EIO;
        return -1;
    }

    /* check checksum */
    if (ctx->record.data[ctx->record.reclen-1] != 0/*optional*/) {
        for (i=0;i < 3;i++)
            sum += tmp[i];
        for (i=0;i < ctx->record.reclen;i++)
            sum += ctx->record.data[i];

        if (sum != 0) {
            ctx->last_error = "Reading OMF record checksum failed";
            errno = EIO;
            return -1;
        }
    }

    /* remember LIBHEAD block size */
    if (ctx->record.rectype == 0xF0/*LIBHEAD*/) {
        if (ctx->library_block_size == 0) {
            // and the length of the record defines the block size that modules within are aligned by
            ctx->library_block_size = ctx->record.reclen + 3;
        }
        else {
            ctx->last_error = "LIBHEAD defined again";
            errno = EIO;
            return -1;
        }
    }

    ctx->record.reclen--; // omit checksum from reclen
    return 1;
}

int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int len;

    len = omf_record_get_lenstr(templstr,sizeof(templstr),rec);
    if (len < 0) return -1;

    if (cstr_set_n(&ctx->THEADR,templstr,len) < 0)
        return -1;

    return 0;
}

int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_lnames_context_get_next_add_index(&ctx->LNAMEs);
    int len;

    while (!omf_record_eof(rec)) {
        len = omf_record_get_lenstr(templstr,sizeof(templstr),rec);
        if (len < 0) return -1;

        if (omf_lnames_context_add_name(&ctx->LNAMEs,templstr,len) < 0)
            return -1;
    }

    return first_entry;
}

int omf_context_parse_SEGDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_segdefs_context_get_next_add_index(&ctx->SEGDEFs);
    struct omf_segdef_t *segdef = omf_segdefs_context_add_segdef(&ctx->SEGDEFs);

    if (segdef == NULL)
        return -1;

    // read segment attributes
    //   alignment      [7:5]
    //   combination    [4:2]
    //   big (max)      [1:1]
    //   use32          [0:0]
    segdef->attr.f.raw = omf_record_get_byte(rec);
    // frame number (conditional)
    if (segdef->attr.f.f.alignment == 0) {
        segdef->attr.frame_number = omf_record_get_word(rec);
        segdef->attr.offset = omf_record_get_byte(rec);
    }

    if (omf_record_eof(rec))
        return -1;

    if (rec->rectype & 1/*32-bit version*/)
        segdef->segment_length = omf_record_get_dword(rec);
    else
        segdef->segment_length = omf_record_get_word(rec);

    segdef->segment_name_index = omf_record_get_index(rec);
    segdef->class_name_index = omf_record_get_index(rec);

    if (omf_record_eof(rec))
        return -1;

    segdef->overlay_name_index = omf_record_get_index(rec);
    return first_entry;
}

//================================== PROGRAM ================================

static char*                            in_file = NULL;   

struct omf_context_t*                   omf_state = NULL;

static void help(void) {
    fprintf(stderr,"omfdump [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to dump\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
}

void dump_LNAMES(const struct omf_context_t * const ctx,unsigned int first_newest) {
    unsigned int i = first_newest;

    if (i == 0)
        return;

    printf("LNAMES (from %u):",i);
    while (i <= omf_lnames_context_get_highest_index(&ctx->LNAMEs)) {
        const char *p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

        if (p != NULL)
            printf(" [%u]: \"%s\"",i,p);
        else
            printf(" [%u]: (null)",i);

        i++;
    }
    printf("\n");
}

void dump_THEADR(const struct omf_context_t * const ctx) {
    printf("* THEADR: ");
    if (ctx->THEADR != NULL)
        printf("\"%s\"",ctx->THEADR);
    else
        printf("(none)\n");

    printf("\n");
}

void dump_SEGDEF(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,i);

    printf("SEGDEF (%u):\n",i);
    if (segdef != NULL) {
        printf("    Alignment=%u combination=%u big=%u frame=%u offset=%u use%u\n",
            segdef->attr.f.f.alignment,
            segdef->attr.f.f.combination,
            segdef->attr.f.f.big_segment,
            segdef->attr.f.f.use32?32U:16U,
            segdef->attr.frame_number,
            segdef->attr.offset);
        printf("    Length=%lu name=\"%s\"(%u) class=\"%s\"(%u) overlay=\"%s\"(%u)\n",
            (unsigned long)segdef->segment_length,
            omf_lnames_context_get_name(&ctx->LNAMEs,segdef->segment_name_index),
            segdef->segment_name_index,
            omf_lnames_context_get_name(&ctx->LNAMEs,segdef->class_name_index),
            segdef->class_name_index,
            omf_lnames_context_get_name(&ctx->LNAMEs,segdef->overlay_name_index),
            segdef->overlay_name_index);
    }
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
            dump_SEGDEF(omf_state,i);
    }

    printf("----END-----\n");
}

int main(int argc,char **argv) {
    unsigned char dumpstate = 0;
    unsigned char diddump = 0;
    unsigned char verbose = 0;
    int i,fd,ret;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                in_file = argv[i++];
                if (in_file == NULL) return 1;
            }
            else if (!strcmp(a,"v")) {
                verbose = 1;
            }
            else if (!strcmp(a,"d")) {
                dumpstate = 1;
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

    // prepare parsing
    if ((omf_state=omf_context_create()) == NULL) {
        fprintf(stderr,"Failed to init OMF parsing state\n");
        return 1;
    }
    omf_state->flags.verbose = (verbose > 0);

    if (in_file == NULL) {
        help();
        return 1;
    }

    fd = open(in_file,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
        return 1;
    }

    omf_context_begin_file(omf_state);

    do {
        ret = omf_context_read_fd(omf_state,fd);
        if (ret == 0) {
            if (omf_record_is_modend(&omf_state->record)) {
                if (dumpstate && !diddump) {
                    my_dumpstate(omf_state);
                    diddump = 1;
                }

                ret = omf_context_next_lib_module_fd(omf_state,fd);
                if (ret < 0) {
                    printf("Unable to advance to next .LIB module, %s\n",strerror(errno));
                    if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
                }
                else if (ret > 0) {
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

        printf("OMF record type=0x%02x (%s: %s) length=%u offset=%lu blocksize=%u\n",
                omf_state->record.rectype,
                omf_rectype_to_str(omf_state->record.rectype),
                omf_rectype_to_str_long(omf_state->record.rectype),
                omf_state->record.reclen,
                omf_state->record.rec_file_offset,
                omf_state->library_block_size);

        switch (omf_state->record.rectype) {
            case OMF_RECTYPE_THEADR:/*0x80*/
                if (omf_context_parse_THEADR(omf_state,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing THEADR\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_THEADR(omf_state);

                break;
            case OMF_RECTYPE_LNAMES:/*0x96*/{
                int first_new_lname;

                if ((first_new_lname=omf_context_parse_LNAMES(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing LNAMES\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_LNAMES(omf_state,(unsigned int)first_new_lname);

                } break;
            case OMF_RECTYPE_SEGDEF:/*0x98*/
            case OMF_RECTYPE_SEGDEF32:/*0x99*/{
                int first_new_segdef;

                if ((first_new_segdef=omf_context_parse_SEGDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing SEGDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_SEGDEF(omf_state,(unsigned int)first_new_segdef);

                } break;
        }
#if 0
            switch (omf_rectype) {
                case 0x82:/* LHEADR */
                    dump_LHEADR();
                    break;
                case 0x88:/* COMENT */
                    dump_COMENT();
                    break;
                case 0x8A:/* MODEND */
                case 0x8B:/* MODEND32 */
                    dump_MODEND(omf_rectype&1);
                    break;
                case 0x8C:/* EXTDEF */
                    dump_EXTDEF();
                    break;
                case 0x90:/* PUBDEF */
                case 0x91:/* PUBDEF32 */
                    dump_PUBDEF(omf_rectype&1);
                    break;
                case 0x9A:/* GRPDEF */
                case 0x9B:/* GRPDEF32 */
                    dump_GRPDEF(omf_rectype&1);
                    break;
                case 0x9C:/* FIXUPP */
                case 0x9D:/* FIXUPP32 */
                    dump_FIXUPP(omf_rectype&1);
                    break;
                case 0xA0:/* LEDATA */
                case 0xA1:/* LEDATA32 */
                    dump_LEDATA(omf_rectype&1);
                    break;
                case 0xA2:/* LIDATA */
                case 0xA3:/* LIDATA32 */
                    dump_LIDATA(omf_rectype&1);
                    break;
                case 0xB4:/* LEXTDEF */
                case 0xB5:/* LEXTDEF32 */
                    dump_LEXTDEF(omf_rectype&1);
                    break;
                case 0xB6:/* LPUBDEF */
                case 0xB7:/* LPUBDEF32 */
                    dump_LPUBDEF(omf_rectype&1);
                    break;
                case 0xF0:/* LIBHEAD */
                    dump_LIBHEAD();
                    break;
                case 0xF1:/* LIBEND */
                    dump_LIBEND();
                    break;
                default:
                    printf("** do not yet support\n");
                    break;
            }

            omfrec_checkrange();
            if (omf_rectype == 0xF1) break; // stop at LIBEND. there's non-OMF records that usually follow it.
#endif
    } while (1);

    if (dumpstate && !diddump) {
        my_dumpstate(omf_state);
        diddump = 1;
    }

    omf_context_clear(omf_state);
    omf_state = omf_context_destroy(omf_state);
    close(fd);
    return 0;
}

