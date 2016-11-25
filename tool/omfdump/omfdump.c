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

#ifndef O_BINARY
#define O_BINARY (0)
#endif

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

//static char                 tempstr[257];

#define OMF_RECTYPE_THEADR      (0x80)

#define OMF_RECTYPE_LNAMES      (0x96)

//================================== cstr ================================

int cstr_set_n(char ** const p,const char * const str,const size_t strl) {
    char *x;

    if (*p != NULL) {
        free(*p);
        *p = NULL;
    }

    x = malloc(strl+1);
    if (x == NULL) return -1;
    memcpy(x,str,strl);
    x[strl] = 0;
    *p = x;
    return 0;
}

void cstr_free(char ** const p) {
    if (*p != NULL) {
        free(*p);
        *p = NULL;
    }
}

//================================== records ================================

struct omf_record_t {
    unsigned char           rectype;
    unsigned short          reclen;             // amount of data in data, reclen < data_alloc not including checksum
    unsigned short          recpos;             // read/write position
    unsigned char*          data;               // data if != NULL. checksum is at data[reclen]
    size_t                  data_alloc;         // amount of data allocated if data != NULL or amount TO alloc if data == NULL

    unsigned long           rec_file_offset;    // file offset of record (~0UL if undefined)
};

unsigned char omf_record_is_modend(const struct omf_record_t * const rec) {
    return ((rec->rectype&0xFE) == 0x8A); // MODEND 0x8A or 0x8B
}

void omf_record_init(struct omf_record_t * const rec) {
    rec->rectype = 0;
    rec->reclen = 0;
    rec->data = NULL;
    rec->data_alloc = 4096; // OMF spec says 1024
    rec->rec_file_offset = (~0UL);
}

void omf_record_data_free(struct omf_record_t * const rec) {
    if (rec->data != NULL) {
        free(rec->data);
        rec->data = NULL;
    }
    rec->reclen = 0;
    rec->rectype = 0;
}

int omf_record_data_alloc(struct omf_record_t * const rec,size_t sz) {
    if (sz > (size_t)0xFF00U) {
        errno = ERANGE;
        return -1;
    }

    if (rec->data != NULL) {
        if (sz == 0 || sz == rec->data_alloc)
            return 0;

        errno = EINVAL;//cannot realloc
        return -1;
    }

    if (sz == 0) // default?
        sz = 4096;

    rec->data = malloc(sz);
    if (rec->data == NULL)
        return -1; // malloc sets errno

    rec->data_alloc = sz;
    rec->reclen = 0;
    rec->recpos = 0;
    return 0;
}

void omf_record_clear(struct omf_record_t * const rec) {
    rec->recpos = 0;
    rec->reclen = 0;
}

unsigned short omf_record_lseek(struct omf_record_t * const rec,unsigned short pos) {
    if (rec->data == NULL)
        rec->recpos = 0;
    else if (pos > rec->reclen)
        rec->recpos = rec->reclen;
    else
        rec->recpos = pos;

    return rec->recpos;
}

size_t omf_record_can_write(const struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0;
    if (rec->recpos >= rec->data_alloc)
        return 0;

    return (size_t)(rec->data_alloc - rec->recpos);
}

size_t omf_record_data_available(const struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0;
    if (rec->recpos >= rec->reclen)
        return 0;

    return (size_t)(rec->reclen - rec->recpos);
}

static inline unsigned char omf_record_eof(const struct omf_record_t * const rec) {
    return (omf_record_data_available(rec) == 0);
}

unsigned char omf_record_get_byte_fast(struct omf_record_t * const rec) {
    unsigned char c;

    c = *((unsigned char*)(rec->data + rec->recpos));
    rec->recpos++;
    return c;
}

unsigned char omf_record_get_byte(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFU;
    if ((rec->recpos+1U) > rec->reclen)
        return 0xFFU;

    return omf_record_get_byte_fast(rec);
}

unsigned short omf_record_get_word_fast(struct omf_record_t * const rec) {
    unsigned short c;

    c = *((unsigned short*)(rec->data + rec->recpos));
    rec->recpos += 2;
    return c;
}

unsigned short omf_record_get_word(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFFFU;
    if ((rec->recpos+2U) > rec->reclen)
        return 0xFFFFU;

    return omf_record_get_word_fast(rec);
}

unsigned long omf_record_get_dword_fast(struct omf_record_t * const rec) {
    unsigned long c;

    c = *((unsigned long*)(rec->data + rec->recpos));
    rec->recpos += 4;
    return c;
}

unsigned long omf_record_get_dword(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFFFU;
    if ((rec->recpos+4U) > rec->reclen)
        return 0xFFFFU;

    return omf_record_get_dword_fast(rec);
}

unsigned int omf_record_get_index(struct omf_record_t * const rec) {
    unsigned int t;

    /* 1 or 2 bytes.
       1 byte if less than 0x80
       2 bytes if more than 0x7F
       
       1 byte encoding (0x00-0x7F)  (value & 0x7F) 
       2 byte encoding (>0x80)      ((value & 0x7F) | 0x80) (value >> 8)*/
    if (omf_record_eof(rec)) return 0;
    t = omf_record_get_byte_fast(rec);
    if (t & 0x80) {
        t = (t & 0x7F) << 8U;
        if (omf_record_eof(rec)) return 0;
        t += omf_record_get_byte_fast(rec);
    }

    return t;
}

int omf_record_read_data(unsigned char *dst,unsigned int len,struct omf_record_t *rec) {
    unsigned int avail;

    avail = omf_record_data_available(rec);
    if (len > avail) len = avail;

    if (len != 0) {
        memcpy(dst,rec->data+rec->recpos,len);
        rec->recpos += len;
    }

    return len;
}

int omf_record_get_lenstr(char *dst,const size_t dstmax,struct omf_record_t *rec) {
    unsigned char len;

    len = omf_record_get_byte(rec);
    if ((len+1U) > dstmax) {
        errno = ENOSPC;
        return -1;
    }

    if (omf_record_read_data((unsigned char*)dst,len,rec) < len) {
        errno = EIO;
        return -1;
    }

    dst[len] = 0;
    return len;
}

void omf_record_free(struct omf_record_t * const rec) {
    omf_record_data_free(rec);
}

//================================== LNAMES ================================

/* LNAMES collection */
struct omf_lnames_context_t {
    char**               omf_LNAMES;
    unsigned int         omf_LNAMES_count;
    unsigned int         omf_LNAMES_alloc;
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

const char *omf_lnames_context_get_name(struct omf_lnames_context_t * const ctx,unsigned int i) {
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

const char *omf_lnames_context_get_name_safe(struct omf_lnames_context_t * const ctx,unsigned int i) {
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

//================================== OMF ================================

struct omf_context_t {
    const char*                         last_error;
    struct omf_lnames_context_t         LNAMEs;
    struct omf_record_t                 record; // reading, during parsing
    unsigned short                      library_block_size;// is .LIB archive if nonzero
    char*                               THEADR;
    struct {
        unsigned int                    verbose:1;
    } flags;
};

void omf_context_init(struct omf_context_t * const ctx) {
    omf_lnames_context_init(&ctx->LNAMEs);
    omf_record_init(&ctx->record);
    ctx->last_error = NULL;
    ctx->flags.verbose = 0;
    ctx->library_block_size = 0;
    ctx->THEADR = NULL;
}

void omf_context_free(struct omf_context_t * const ctx) {
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
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_begin_module(struct omf_context_t * const ctx) {
    omf_lnames_context_free_names(&ctx->LNAMEs);
    cstr_free(&ctx->THEADR);
}

void omf_context_clear(struct omf_context_t * const ctx) {
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
}

//================================== PROGRAM ================================

struct omf_context_t*                   omf_state = NULL;

#if 0
struct omf_SEGDEF_t {
    unsigned char           nameidx;
};

#define MAX_SEGDEFS         255
static struct omf_SEGDEF_t  omf_SEGDEFS[MAX_SEGDEFS];
static unsigned int         omf_SEGDEFS_count = 0;

static struct omf_SEGDEF_t *omf_SEGDEFS_add(const unsigned char nameidx) {
    struct omf_SEGDEF_t *rec;

    if (nameidx == 0)
        return NULL;
    if (omf_SEGDEFS_count >= MAX_SEGDEFS)
        return NULL;

    rec = &omf_SEGDEFS[omf_SEGDEFS_count++];
    memset(rec,0,sizeof(*rec));
    rec->nameidx = nameidx;
    return rec;
}

static struct omf_SEGDEF_t *omf_get_SEGDEF(const unsigned int i) {
    if (i == 0 || i > omf_SEGDEFS_count)
        return NULL;

    return &omf_SEGDEFS[i-1];
}

static const char *omf_get_SEGDEF_name(const unsigned int i) {
    struct omf_SEGDEF_t *rec = omf_get_SEGDEF(i);
    unsigned char idx = (rec != NULL) ? rec->nameidx : 0;
    return omf_lnames_context_get_name(&omf_state->LNAMEs,idx);
}

static const char *omf_get_SEGDEF_name_safe(const unsigned int i) {
    const char *name = omf_get_SEGDEF_name(i);
    return (name != NULL) ? name : "[ERANGE]";
}

static void omf_SEGDEF_clear(void) {
    omf_SEGDEFS_count = 0;
}

struct omf_GRPDEF_t {
    unsigned char           nameidx;
};

#define MAX_GRPDEFS         255
static struct omf_GRPDEF_t  omf_GRPDEFS[MAX_GRPDEFS];
static unsigned int         omf_GRPDEFS_count = 0;

static struct omf_GRPDEF_t *omf_GRPDEFS_add(const unsigned char nameidx) {
    struct omf_GRPDEF_t *rec;

    if (nameidx == 0)
        return NULL;
    if (omf_GRPDEFS_count >= MAX_GRPDEFS)
        return NULL;

    rec = &omf_GRPDEFS[omf_GRPDEFS_count++];
    memset(rec,0,sizeof(*rec));
    rec->nameidx = nameidx;
    return rec;
}

static struct omf_GRPDEF_t *omf_get_GRPDEF(const unsigned int i) {
    if (i == 0 || i > omf_GRPDEFS_count)
        return NULL;

    return &omf_GRPDEFS[i-1];
}

static const char *omf_get_GRPDEF_name(const unsigned int i) {
    struct omf_GRPDEF_t *rec = omf_get_GRPDEF(i);
    unsigned char idx = (rec != NULL) ? rec->nameidx : 0;
    return omf_lnames_context_get_name(&omf_state->LNAMEs,idx);
}

static const char *omf_get_GRPDEF_name_safe(const unsigned int i) {
    const char *name = omf_get_GRPDEF_name(i);
    return (name != NULL) ? name : "[none]";
}

static void omf_GRPDEF_clear(void) {
    omf_GRPDEFS_count = 0;
}

/* EXTDEF collection */
#define MAX_EXTDEF          255
static char*                omf_EXTDEF[MAX_EXTDEF];
static unsigned int         omf_EXTDEF_count = 0;

static const char *omf_get_EXTDEF(const unsigned int i) {
    if (i == 0 || i > omf_EXTDEF_count) /* EXTDEFs are 1-based */
        return NULL;

    return omf_EXTDEF[i-1];
}

static const char *omf_get_EXTDEF_safe(const unsigned int i) {
    const char *r = omf_get_EXTDEF(i);

    return (r != NULL) ? r : "[ERANGE]";
}

static void omf_EXTDEF_clear(void) {
    while (omf_EXTDEF_count > 0) {
        omf_EXTDEF_count--;

        if (omf_EXTDEF[omf_EXTDEF_count] != NULL) {
            free(omf_EXTDEF[omf_EXTDEF_count]);
            omf_EXTDEF[omf_EXTDEF_count] = NULL;
        }
    }
}

static void omf_EXTDEF_add(const char *name) {
    size_t len = strlen(name);
    char *p;

    if (name == NULL)
        return;
    if (omf_EXTDEF_count >= MAX_EXTDEF)
        return;

    p = malloc(len+1);
    if (p == NULL)
        return;

    memcpy(p,name,len+1);/* name + NUL */
    omf_EXTDEF[omf_EXTDEF_count++] = p;
}
#endif

#if 0
static void omf_reset(void) {
    omf_context_clear(omf_state);
    omf_SEGDEF_clear();
    omf_GRPDEF_clear();
    omf_EXTDEF_clear();
}

static inline unsigned int omfrec_eof(void) {
    return (omf_recpos >= omf_reclen);
}

static inline unsigned int omfrec_avail(void) {
    return (omf_reclen - omf_recpos);
}

void omfrec_end(void) {
    omf_recpos = omf_reclen;
}

static inline unsigned char omfrec_gb(void) {
    return omf_record[omf_recpos++];
}

static inline unsigned int omfrec_gw(void) {
    unsigned int v = *((unsigned short*)(omf_record+omf_recpos));
    omf_recpos += 2;
    return v;
}

static inline unsigned int omfrec_gindex(void) {
    /* 1 or 2 bytes.
       1 byte if less than 0x80
       2 bytes if more than 0x7F */
    unsigned int t;

    if (omfrec_eof()) return 0;
    t = omfrec_gb();
    if (t & 0x80) {
        t = (t & 0x7F) << 8U;
        if (omfrec_eof()) return 0;
        t += omfrec_gb();
    }

    return t;
}

static inline unsigned long omfrec_gd(void) {
    unsigned long v = *((uint32_t*)(omf_record+omf_recpos));
    omf_recpos += 4;
    return v;
}

void omfrec_read(char *dst,unsigned int len) {
    if (len > 0) {
        memcpy(dst,omf_record+omf_recpos,len);
        omf_recpos += len;
    }
}

void omfrec_checkrange(void) {
    if (omf_recpos > omf_reclen) {
        fprintf(stderr,"ERROR: Overread occurred\n");
        abort();
    }
}
#endif

static char*                in_file = NULL;   

static void help(void) {
    fprintf(stderr,"omfdump [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to dump\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
}

#if 0
static int omf_lib_next_block(int fd,unsigned long checkofs) {
    unsigned long endoff;

    if (omf_lib_blocksize == 0)
        return 0;

    if ((off_t)lseek(fd,checkofs,SEEK_SET) != (off_t)checkofs)
        return 0;

    checkofs = (checkofs + (unsigned long)omf_lib_blocksize - 1UL) & (~((unsigned long)omf_lib_blocksize - 1UL));
    endoff = lseek(fd,0,SEEK_END);
    if (checkofs > endoff)
        return 0;

    if ((off_t)lseek(fd,checkofs,SEEK_SET) != (off_t)checkofs)
        return 0;

    return 1;
}
#endif

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

#if 0
unsigned int omfrec_get_remstr(char * const dst,const size_t dstmax) {
    unsigned int len;

    if (dstmax == 0)
        return 0;

    dst[0] = 0;
    if (omfrec_eof())
        return 0;

    len = omfrec_avail();
    if (len == 0 || len > 255)
        return 0;

    omfrec_read(dst,len);
    dst[len] = 0;
    return len;
}

unsigned int omfrec_get_lenstr(char * const dst,const size_t dstmax) {
    unsigned char len;

    if (dstmax == 0)
        return 0;

    dst[0] = 0;
    if (omfrec_eof())
        return 0;

    len = omfrec_gb();
    if (len > omfrec_avail() || (len+1U/*NUL*/) > dstmax) {
        omfrec_end();
        return 0;
    }

    if (len > 0) {
        omfrec_read(dst,len);
        dst[len] = 0;
    }

    return len;
}
#endif

static char templstr[255+1/*NUL*/];

int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int len;

    len = omf_record_get_lenstr(templstr,sizeof(templstr),rec);
    if (len < 0) return -1;

    if (cstr_set_n(&ctx->THEADR,templstr,len) < 0)
        return -1;

    return 0;
}

int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = ctx->LNAMEs.omf_LNAMES_count + 1; // 1-based, remember?
    int len;

    while (!omf_record_eof(rec)) {
        len = omf_record_get_lenstr(templstr,sizeof(templstr),rec);
        if (len < 0) return -1;

        if (omf_lnames_context_add_name(&ctx->LNAMEs,templstr,len) < 0)
            return -1;
    }

    return first_entry;
}

#if 0
void dump_THEADR(void) {
    /* omf_record+0: length of the string */
    /* omf_record+1: ASCII string */
    omfrec_get_lenstr(tempstr,sizeof(tempstr));
    printf("    Name of the object: \"%s\"\n",tempstr);
}

void dump_LHEADR(void) {
    /* omf_record+0: length of the string */
    /* omf_record+1: ASCII string */
    omfrec_get_lenstr(tempstr,sizeof(tempstr));
    printf("    Name of the object in library: \"%s\"\n",tempstr);
}

const char *omfrec_COMENT_cclass_to_str(unsigned char cc) {
    switch (cc) {
        case 0x00:  return "Translator";
        case 0x01:  return "Intel copyright";
        case 0x81:  return "Library specifier";
        case 0x9C:  return "MS-DOS version";
        case 0x9D:  return "Memory model";
        case 0x9E:  return "DOSSEG";
        case 0x9F:  return "Default library search name";
        case 0xA0:  return "OMF extension";
        case 0xA1:  return "New OMF extension";
        case 0xA2:  return "Link Pass Separator";
        case 0xA3:  return "Library module comment record";
        case 0xA4:  return "Executable string";
        case 0xA6:  return "Incremental compilation error";
        case 0xA7:  return "No segment padding";
        case 0xA8:  return "Weak extern record";
        case 0xA9:  return "Lazy extern record";
        case 0xDA:  return "Comment";
        case 0xDB:  return "Compiler (pragma)";
        case 0xDC:  return "Date (pragma)";
        case 0xDD:  return "Timestamp (pragma)";
        case 0xDF:  return "User (pragma)";
        case 0xE9:  return "Dependency file";
        default:    break;
    };

    return "?";
}

void dump_COMENT(void) {
    unsigned char ctype,cclass;

    /* omf_record+0: Comment Type
     * omf_record+1: Comment Class
     * omf_record+2: byte string until end of record */
    if (omfrec_avail() < 2) return;
    ctype = omfrec_gb();
    cclass = omfrec_gb();

    printf("    COMENT Type=0x%02x ( ",ctype);
    if (ctype & 0x80) printf("No-purge ");
    if (ctype & 0x40) printf("No-list ");
    printf(") class=0x%02x (%s): ",cclass,omfrec_COMENT_cclass_to_str(cclass));
    printf("\n");

    if (cclass == 0x9F) {
        /* the rest is the string (no length byte) */
        omfrec_get_remstr(tempstr,sizeof(tempstr));
        printf("        Library name: %s\n",tempstr);
    }
}

// load OMF LNAMEs from record in memory.
// return value is -1 if error, 0 if none loaded.
// if > 0, the return value is the first LNAME index.
int omf_context_load_LNAMEs(struct omf_context_t * const ctx) {
    int baseidx = 0,ret;

    /* TODO: Load from context buffer */

    /* string records, one after the other, until the end of the record */
    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        ret = omf_lnames_context_add_name(&ctx->LNAMEs,tempstr);
        if (ret < 0) return -1;

        if (baseidx == 0)
            baseidx = ret;
    }

    return baseidx;
}

int dump_LNAMES(void) {
    unsigned int i;
    const char *p;
    int baseidx;

    if ((baseidx=omf_context_load_LNAMEs(omf_state)) < 0)
        return -1;

    if (omf_state->flags.verbose) {
        printf("    LNAMEs (starting at %u):",baseidx);
        for (i=baseidx;i <= omf_state->LNAMEs.omf_LNAMES_count;i++) {
            p = omf_lnames_context_get_name(&omf_state->LNAMEs,i);

            if (p != NULL)
                printf(" \"%s\"",p);
            else
                printf(" (null)");
        }

        printf("\n");
    }

    return baseidx;
}

void dump_EXTDEF(void) {
    unsigned int typidx;

    printf("    EXTDEF:\n");
    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        typidx = omfrec_gindex(); /* 1 or 2 bytes */

        printf("        '%s' typidx=%u\n",tempstr,typidx);

        omf_EXTDEF_add(tempstr);
    }
}

void dump_LEXTDEF(const unsigned char b32) {
    unsigned int typidx;

    (void)b32; /* unused */

    printf("    LEXTDEF:\n");
    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        typidx = omfrec_gindex(); /* 1 or 2 bytes */

        printf("        '%s' typidx=%u\n",tempstr,typidx);

        omf_EXTDEF_add(tempstr);
    }
}

void dump_PUBDEF(const unsigned char b32) {
    unsigned int base_segment_index = 0;
    unsigned int base_group_index = 0;
    unsigned int base_frame = 0;
    unsigned long puboff;
    unsigned int typidx;

    if (omfrec_avail() < 2) return;
    base_group_index = omfrec_gb();
    base_segment_index = omfrec_gb();

    if (omfrec_avail() < 2) return;
    if (base_segment_index == 0)
        base_frame = omfrec_gw();

    printf("    PUBDEF%u: basegroupidx=\"%s\"(%u) basesegidx=\"%s\"(%u) baseframe=%u\n",
        b32?32:16,omf_get_GRPDEF_name_safe(base_group_index),base_group_index,
        omf_get_SEGDEF_name_safe(base_segment_index),base_segment_index,
        base_frame);

    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        if (b32) {
            if (omfrec_avail() < (4+1)) break;
            puboff = omfrec_gd();
            typidx = omfrec_gb();
        }
        else {
            if (omfrec_avail() < (2+1)) break;
            puboff = omfrec_gw();
            typidx = omfrec_gb();
        }

        printf("        '%s' offset=%lu(0x%lx) typeidx=%u\n",tempstr,puboff,puboff,typidx);
    }
}

void dump_LPUBDEF(const unsigned char b32) {
    unsigned int base_segment_index = 0;
    unsigned int base_group_index = 0;
    unsigned int base_frame = 0;
    unsigned long puboff;
    unsigned int typidx;

    if (omfrec_avail() < 2) return;
    base_group_index = omfrec_gb();
    base_segment_index = omfrec_gb();

    if (omfrec_avail() < 2) return;
    if (base_segment_index == 0)
        base_frame = omfrec_gw();

    printf("    LPUBDEF%u: basegroupidx=\"%s\"(%u) basesegidx=\"%s\"(%u) baseframe=%u\n",
        b32?32:16,omf_get_GRPDEF_name_safe(base_group_index),base_group_index,
        omf_get_SEGDEF_name_safe(base_segment_index),base_segment_index,
        base_frame);

    while (!omfrec_eof()) {
        omfrec_get_lenstr(tempstr,sizeof(tempstr));

        if (b32) {
            if (omfrec_avail() < (4+1)) break;
            puboff = omfrec_gd();
            typidx = omfrec_gb();
        }
        else {
            if (omfrec_avail() < (2+1)) break;
            puboff = omfrec_gw();
            typidx = omfrec_gb();
        }

        printf("        '%s' offset=%lu(0x%lx) typeidx=%u\n",tempstr,puboff,puboff,typidx);
    }
}

void dump_FIXUPP(const unsigned char b32) {
    unsigned int c;

    printf("    FIXUPP%u:\n",b32?32:16);

    /* whoo, dense structures */
    while (!omfrec_eof()) {
        c = omfrec_gb();

        if ((c&0xA0/*0x80+0x20*/) == 0x00/* 0 D 0 x x x x x */) {
            printf("        * error THREAD records not supported\n");
            break;
        }
        else if (c&0x80) {
            if (omfrec_eof()) break;

            {
                unsigned long target_disp;
                unsigned char fix_f,fix_frame,fix_t,fix_p,fix_target;
                unsigned char fixdata;
                unsigned char segrel_fixup = (c >> 6) & 1;
                unsigned char locat = (c >> 2) & 0xF;
                unsigned int recoff = ((c & 3U) << 8U) + (unsigned int)omfrec_gb();

                printf("        FIXUP %s loctofix=",segrel_fixup?"SEG-RELATIVE":"SELF-RELATIVE");
                switch (locat) {
                    case 0: printf("LOBYTE"); break;
                    case 1: printf("16BIT-OFFSET"); break;
                    case 2: printf("16BIT-SEGBASE"); break;
                    case 3: printf("16SEG:16OFS-FAR-POINTER"); break;
                    case 4: printf("HIBYTE"); break;
                    case 5: printf("16BIT-LOADER-OFFSET"); break;
                    case 9: printf("32BIT-OFFSET"); break;
                    case 11:printf("16SEG:32OFS-FAR-POINTER"); break;
                    case 13:printf("32BIT-LOADER-OFFSET"); break;
                };
                printf(" datarecofs=(rel)0x%lX,(abs)0x%lX",(unsigned long)recoff,(unsigned long)recoff+last_LEDATA_data_offset);
                printf("\n");

                // FIXME: Is "Fix Data" conditional? If so, when does it happen??
                //        The OMF spec doesn't say! This is a part of the spec
                //        where the denseness and conditional nature is VERY
                //        problematic because the OMF spec is very bad at explaining
                //        WHEN these conditional fields appear.
                if (omfrec_eof()) break;
                fixdata = omfrec_gb();

                fix_f = (fixdata >> 7) & 1;     /* [7:7] */
                fix_frame = (fixdata >> 4) & 7; /* [6:4] */
                fix_t = (fixdata >> 3) & 1;     /* [3:3] */
                fix_p = (fixdata >> 2) & 1;     /* [2:2] */
                fix_target = (fixdata & 3);     /* [1:0] */

                printf("            FD=0x%02X ",fixdata);
                if (fix_f)
                    printf("F=1 frame=%u",fix_frame);
                else
                    printf("F=0 framemethod=%u",fix_frame);

                if (!fix_f) {
                    // again, if the OMF spec doesn't explain this and I have to read VirtualBox source code
                    // to get this that says how reliable your spec is.
                    switch (fix_frame) {
                        case 0: // F0: segment
                            printf(" F0:frame=segment");
                            break;
                        case 1: // F1: group
                            printf(" F1:frame=group");
                            break;
                        case 2: // F2: external symbol
                            printf(" F2:frame=external-symbol");
                            break;
                        case 4: // F4: frame = source
                            printf(" F4:frame=source");
                            break;
                        case 5: // F5: frame = target
                            printf(" F5:frame=target");
                            break;
                    }
                }

                if (!fix_f && fix_frame < 4) {
                    if (omfrec_eof()) break;

                    c = omfrec_gindex();
                    printf(" framedatum=%u",c);

                    switch (fix_frame) {
                        case 0: // segment
                            printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                            break;
                        case 1: // group
                            printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                            break;
                        case 2: // external symbol
                            printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                            break;
                    }
                }

                /* FIXME: WHEN is the Target Datum field prsent???? This is a shitty guess! The OMF spec doesn't say! */
                // NTS: To the people who wrote the OMF spec: your doc is confusing. The fact I had to read VirtualBox
                //      source code for clarification means your spec needs clarification.
                if (fix_t) {
                    printf(" target=%u",fix_target);
                }
                else {
                    if (omfrec_eof()) break;

                    switch (fix_target) {
                        case 0: // T0/T4: Target = segment
                            printf(" T0:target=segment");
                            break;
                        case 1: // T1/T5: Target = segment group
                            printf(" T0:target=segment-group");
                            break;
                        case 2: // T2/T6: Target = external symbol
                            printf(" T0:target=extern-sym");
                            break;
                    };

                    c = omfrec_gindex();
                    printf(" targetdatum=%u",c);

                    switch (fix_target) {
                        case 0: // T0/T4: Target = segment
                            printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                            break;
                        case 1: // T1/T5: Target = segment group
                            printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                            break;
                        case 2: // T2/T6: Target = external symbol
                            printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                            break;
                    };
                }

                if (!fix_p) {
                    if (b32) {
                        if (omfrec_avail() < 4) break;
                        target_disp = omfrec_gd();
                    }
                    else {
                        if (omfrec_avail() < 2) break;
                        target_disp = omfrec_gw();
                    }

                    printf(" targetdisp=%lu(0x%lx)",target_disp,target_disp);
                }

                printf("\n");
            }
        }
        else {
            printf("        * error unknown record\n");
            break;
        }
    }
}

/* NTS: I'm seeing Open Watcom emit .LIB files where more than one GRPDEF can be
 *      emitted with the same name, different segments listed. This is legal,
 *      it means you just combine the segment lists together (OMF spec). */
void dump_GRPDEF(const unsigned char b32) {
    unsigned int grpnamidx;
    unsigned int index;
    unsigned char segdefidx;

    (void)b32; // unused

    if (omfrec_eof()) return;
    grpnamidx = omfrec_gindex();
    printf("    GRPDEF nameidx=\"%s\"(%u):\n",omf_lnames_context_get_name_safe(&omf_state->LNAMEs,grpnamidx),grpnamidx);

    omf_GRPDEFS_add(grpnamidx);

    while (!omfrec_eof()) {
        index = omfrec_gb();

        if (index == 0xFF) {
            /* segment def index */
            segdefidx = omfrec_gindex();
            printf("        SEGDEF index=\"%s\"(%u)\n",omf_get_SEGDEF_name_safe(segdefidx),segdefidx);
        }
        else {
            printf("* Whoops, don't know how to handle type 0x%02x\n",index);
            break;
        }
    }
}

void dump_SEGDEF(const unsigned char b32) {
    unsigned char align_f;
    unsigned char comb_f;
    unsigned char use32;
    unsigned char big;
    unsigned short frame_number;
    unsigned char offset;
    unsigned long seg_length;
    unsigned short segnamidx;
    unsigned short classnamidx;
    unsigned short ovlnamidx;
    unsigned char c;

    if (omfrec_eof()) return;
    c = omfrec_gb();
    align_f = (c >> 5) & 7; /* [7:5] = alignment code */
    comb_f = (c >> 2) & 7;  /* [4:2] = combination code */
    big = (c >> 1) & 1;     /* [1:1] = "BIG" meaning the segment is 64K or 4GB exactly and seg length == 0 */
    use32 = (c >> 0) & 1;   /* [0:0] = if set, 32-bit code/data  if clear, 16-bit code/data */

    if (align_f == 0) {
        if (omfrec_avail() < (2+1)) return;
        frame_number = omfrec_gw();
        offset = omfrec_gb();
    }

    if (b32) {
        if (omfrec_avail() < (4+1+1+1)) return;
        seg_length = omfrec_gd();
        segnamidx = omfrec_gindex();
        classnamidx = omfrec_gindex();
        ovlnamidx = omfrec_gindex();
    }
    else {
        if (omfrec_avail() < (2+1+1+1)) return;
        seg_length = omfrec_gw();
        segnamidx = omfrec_gindex();
        classnamidx = omfrec_gindex();
        ovlnamidx = omfrec_gindex();
    }

    printf("    SEGDEF%u: USE%u",b32?32:16,use32?32:16);
    if (big && seg_length == 0) printf(" length=%s(max)",b32?"4GB":"64KB");
    else printf(" length=%lu",seg_length);
    printf(" nameidx=\"%s\"(%u) classidx=\"%s\"(%u) ovlidx=\"%s\"(%u)",
        omf_lnames_context_get_name_safe(&omf_state->LNAMEs,segnamidx),      segnamidx,
        omf_lnames_context_get_name_safe(&omf_state->LNAMEs,classnamidx),    classnamidx,
        omf_lnames_context_get_name_safe(&omf_state->LNAMEs,ovlnamidx),      ovlnamidx);
    printf("\n");

    switch (align_f) {
        case 0:
            printf("        ABSOLUTE frameno=%u offset=%u\n",frame_number,offset);
            break;
        case 1:
            printf("        RELOCATABLE, BYTE ALIGNMENT\n");
            break;
        case 2:
            printf("        RELOCATABLE, WORD (16-bit) ALIGNMENT\n");
            break;
        case 3:
            printf("        RELOCATABLE, PARAGRAPH (16-byte) ALIGNMENT\n");
            break;
        case 4:
            printf("        RELOCATABLE, PAGE ALIGNMENT\n");
            break;
        case 5:
            printf("        RELOCATABLE, DWORD (32-bit) ALIGNMENT\n");
            break;
    };

    switch (comb_f) {
        case 0:
            printf("        PRIVATE, do not combine with any other segment\n");
            break;
        case 2:
        case 4:
        case 7:
            printf("        PUBLIC, combine by appending at aligned offset\n");
            break;
        case 5:
            printf("        STACK, combine by appending at aligned offset\n");
            break;
        case 6:
            printf("        COMMON, combine by overlaying using max size\n");
            break;
    };

    omf_SEGDEFS_add(segnamidx);
}

void dump_LEDATA(const unsigned char b32) {
    unsigned long enum_data_offset,doh;
    unsigned int segment_index;
    unsigned int len,i,colo;
    unsigned char tmp[16];

    if (b32) {
        if (omfrec_avail() < (1+4)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gd();
    }
    else {
        if (omfrec_avail() < (1+2)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gw();
    }

    last_LEDATA_segment_index = segment_index;
    last_LEDATA_data_offset = enum_data_offset;
    printf("    LEDATA%u: segidx=\"%s\"(%u) data_offset=%lu\n",b32?32:16,omf_get_SEGDEF_name_safe(segment_index),segment_index,enum_data_offset);

    doh = enum_data_offset;
    while (!omfrec_eof()) {
        len = omfrec_avail();
        colo = (unsigned int)(doh&0xFUL);
        if (len > (16-colo)) len = (16-colo);
        omfrec_read((char*)tmp+colo,len);

        printf("    @0x%08lx: ",doh);
        for (i=0;i < colo;i++) printf("   ");
        for (   ;i < (colo+len);i++) printf("%02X%c",tmp[i],i==7?'-':' ');
        for (   ;i <  16;i++) printf("   ");
        printf("  ");
        for (i=0;i < colo;i++) printf(" ");
        for (   ;i < (colo+len);i++) {
            if (tmp[i] < 32 || tmp[i] >= 127)
                printf(".");
            else
                printf("%c",tmp[i]);
        }
        printf("\n");

        doh += (unsigned long)len;
    }
}

void dump_LIDATA_indent(unsigned int indent) {
    while (indent-- > 0) printf("    ");
}

int dump_LIDATA_datablock(const unsigned char b32,const unsigned int indent,unsigned long *doh) {
    unsigned long repeat_count;
    unsigned short block_count;

    if (b32) {
        if (omfrec_avail() < (4+2)) return 0;
        repeat_count = omfrec_gd();
        block_count = omfrec_gw();
    }
    else {
        if (omfrec_avail() < (2+2)) return 0;
        repeat_count = omfrec_gw();
        block_count = omfrec_gw();
    }

    dump_LIDATA_indent(indent);
    if (block_count == 0) {
        unsigned long repeat_iter;
        unsigned char rowstart=0;
        unsigned char count;
        unsigned char col;
        unsigned int i,j;
        unsigned char c;
        char row[16];

        if (omfrec_eof()) return 0;
        count = omfrec_gb();

        printf("<content len=%u x %lu>:\n",count,repeat_count);

        /* read into memory */
        if (count != 0) {
            if (omfrec_avail() < count) return 0;
            omfrec_read(tempstr,count);
        }

        for (repeat_iter=0;repeat_iter < repeat_count;repeat_iter++) {
            for (i=0,col=0;i < count;) {
                if (i == 0 || col == 0) {
                    j = (unsigned char)((*doh) & 0xFUL);
                    dump_LIDATA_indent(indent+1);
                    printf("@0x%08lx: ",*doh);
                    while (col < j) {
                        printf("   ");
                        col++;
                    }
                    rowstart = col;
                }

                row[col] = tempstr[i];
                printf("%02X%c",row[col],col==7?'-':' ');
                (*doh)++;
                col++;
                i++;

                if (col >= 16 || i == count) {
                    j = col;
                    while (j < 16) {
                        printf("   ");
                        j++;
                    }

                    j = 0;
                    while (j < rowstart) {
                        printf(" ");
                        j++;
                    }
                    while (j < col) {
                        c = (unsigned char)row[j];
                        j++;

                        if (c >= 32 && c < 127)
                            printf("%c",c);
                        else
                            printf(".");
                    }
                    printf("\n");

                    rowstart = 0;
                    col = 0;
                }
            }
        }
    }
    else {
        /* WARNING: untested! */
        printf("<block x %lu>:\n",repeat_count);
        return dump_LIDATA_datablock(b32,indent+1,doh);
    }

    return 1;
}

void dump_LIDATA(const unsigned char b32) {
    unsigned long enum_data_offset,doh;
    unsigned int segment_index;

    if (b32) {
        if (omfrec_avail() < (1+4)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gd();
    }
    else {
        if (omfrec_avail() < (1+2)) return;
        segment_index = omfrec_gindex();
        enum_data_offset = omfrec_gw();
    }

    last_LEDATA_segment_index = segment_index;
    last_LEDATA_data_offset = enum_data_offset;
    printf("    LIDATA%u: segidx=\"%s\"(%u) data_offset=%lu\n",b32?32:16,omf_get_SEGDEF_name_safe(segment_index),segment_index,enum_data_offset);

    doh = enum_data_offset;
    dump_LIDATA_datablock(b32,2,&doh);
}

void dump_LIBHEAD(void) {
    /* the size of the record determines the block size of the .lib archive.
     * it SHOULD be a power of 2! */
    unsigned int fsz = omf_reclen + 1 + 2 + 1;

    if ((fsz & (fsz - 1U)) != 0) {
        printf("    Unable to determine LIB blocksize (record length %u is not a power of 2)\n",fsz);
        return;
    }

    printf("    LIB blocksize is %u bytes\n",fsz);
    omf_lib_blocksize = fsz;
}

void dump_LIBEND(void) {
    unsigned int fsz = omf_reclen + 1 + 2 + 1;

    printf("    End of LIB archive\n");

    if (omf_lib_blocksize == 0)
        printf("        * WARNING: LIBEND outside LIBHEAD..LIBEND group\n");
    else if (fsz != omf_lib_blocksize)
        printf("        * WARNING: Blocksize not the same size as LIBHEAD\n");

    omf_lib_blocksize = 0;
}

void dump_MODEND(const unsigned char b32) {
    unsigned char fix_f,fix_frame,fix_t,fix_p,fix_target;
    unsigned long target_disp;
    unsigned char module_type;
    unsigned char end_data;
    unsigned int c;

    if (omfrec_eof()) return;
    module_type = omfrec_gb();

    printf("    MODEND%u: module type 0x%02X [ ",b32?32:16,module_type);
    if (module_type & 0x80) printf("MAIN ");
    if (module_type & 0x40) printf("START ");
    if (module_type & 0x01) printf("RELOC-START ");
    printf("]\n");

    if (module_type & 0x40) {
        while (!omfrec_eof()) {
            end_data = omfrec_gb();

            fix_f = (end_data >> 7) & 1;     /* [7:7] */
            fix_frame = (end_data >> 4) & 7; /* [6:4] */
            fix_t = (end_data >> 3) & 1;     /* [3:3] */
            fix_p = (end_data >> 2) & 1;     /* [2:2] */
            fix_target = (end_data & 3);     /* [1:0] */

            printf("        ED=0x%02X ",end_data);
            if (fix_f)
                printf("F=1 frame=%u",fix_frame);
            else
                printf("F=0 framemethod=%u",fix_frame);

            if (!fix_f) {
                // again, if the OMF spec doesn't explain this and I have to read VirtualBox source code
                // to get this that says how reliable your spec is.
                switch (fix_frame) {
                    case 0: // F0: segment
                        printf(" F0:frame=segment");
                        break;
                    case 1: // F1: group
                        printf(" F1:frame=group");
                        break;
                    case 2: // F2: external symbol
                        printf(" F2:frame=external-symbol");
                        break;
                    case 4: // F4: frame = source
                        printf(" F4:frame=source");
                        break;
                    case 5: // F5: frame = target
                        printf(" F5:frame=target");
                        break;
                }
            }

            if (!fix_f && fix_frame < 4) {
                if (omfrec_eof()) break;

                c = omfrec_gindex();
                printf(" framedatum=%u",c);

                switch (fix_frame) {
                    case 0: // segment
                        printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                        break;
                    case 1: // group
                        printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                        break;
                    case 2: // external symbol
                        printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                        break;
                }
            }

            /* FIXME: WHEN is the Target Datum field prsent???? This is a shitty guess! The OMF spec doesn't say! */
            // NTS: To the people who wrote the OMF spec: your doc is confusing. The fact I had to read VirtualBox
            //      source code for clarification means your spec needs clarification.
            if (fix_t) {
                printf(" target=%u",fix_target);
            }
            else {
                if (omfrec_eof()) break;

                switch (fix_target) {
                    case 0: // T0/T4: Target = segment
                        printf(" T0:target=segment");
                        break;
                    case 1: // T1/T5: Target = segment group
                        printf(" T0:target=segment-group");
                        break;
                    case 2: // T2/T6: Target = external symbol
                        printf(" T0:target=extern-sym");
                        break;
                };

                c = omfrec_gindex();
                printf(" targetdatum=%u",c);

                switch (fix_target) {
                    case 0: // T0/T4: Target = segment
                        printf("(\"%s\")",omf_get_SEGDEF_name_safe(c));
                        break;
                    case 1: // T1/T5: Target = segment group
                        printf("(\"%s\")",omf_get_GRPDEF_name_safe(c));
                        break;
                    case 2: // T2/T6: Target = external symbol
                        printf("(\"%s\")",omf_get_EXTDEF_safe(c));
                        break;
                };
            }

            if (!fix_p) {
                if (b32) {
                    if (omfrec_avail() < 4) break;
                    target_disp = omfrec_gd();
                }
                else {
                    if (omfrec_avail() < 2) break;
                    target_disp = omfrec_gw();
                }

                printf(" targetdisp=%lu(0x%lx)",target_disp,target_disp);
            }

            printf("\n");
        }
    }
}
#endif

void dump_LNAMES(const struct omf_context_t * const ctx,unsigned int first_newest) {
    unsigned int i = first_newest;

    if (i == 0)
        return;

    printf("LNAMES (from %u): ",i);
    while (i < (ctx->LNAMEs.omf_LNAMES_count + 1/*based*/)) {
        const char *p = omf_lnames_context_get_name(&omf_state->LNAMEs,i);

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

void my_dumpstate(void) {
    unsigned int i;
    const char *p;

    printf("OBJ dump state:\n");

    if (omf_state->THEADR != NULL)
        printf("* THEADR: \"%s\"\n",omf_state->THEADR);

    if (omf_state->LNAMEs.omf_LNAMES != NULL) {
        printf("* LNAMEs:\n");
        for (i=1;i <= omf_state->LNAMEs.omf_LNAMES_count;i++) {
            p = omf_lnames_context_get_name(&omf_state->LNAMEs,i);

            if (p != NULL)
                printf("   [%u]: \"%s\"\n",i,p);
            else
                printf("   [%u]: (null)\n",i);
        }
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
                    my_dumpstate();
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
        }
#if 0
            switch (omf_rectype) {
                case 0x80:/* THEADR */
                    dump_THEADR();
                    break;
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
                case 0x96:/* LNAMES */
                    if (dump_LNAMES() < 0) return 1;
                    break;
                case 0x98:/* SEGDEF */
                case 0x99:/* SEGDEF32 */
                    dump_SEGDEF(omf_rectype&1);
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
        my_dumpstate();
        diddump = 1;
    }

//    omf_reset();
    omf_context_clear(omf_state);
    omf_state = omf_context_destroy(omf_state);
    close(fd);
    return 0;
}

