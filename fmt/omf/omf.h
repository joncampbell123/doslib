
#ifndef _DOSLIB_OMF_OMFCTX_H
#define _DOSLIB_OMF_OMFCTX_H

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

enum {
    OMF_EXTDEF_TYPE_GLOBAL=0,
    OMF_EXTDEF_TYPE_LOCAL
};

enum {
    OMF_PUBDEF_TYPE_GLOBAL=0,
    OMF_PUBDEF_TYPE_LOCAL
};

enum {
    OMF_SEGDEF_ALIGN_ABSOLUTE=0,
    OMF_SEGDEF_RELOC_BYTE=1,
    OMF_SEGDEF_RELOC_WORD=2,
    OMF_SEGDEF_RELOC_PARA=3,
    OMF_SEGDEF_RELOC_PAGE=4,
    OMF_SEGDEF_RELOC_DWORD=5
};

enum {
    OMF_SEGDEF_COMBINE_PRIVATE=0,
    OMF_SEGDEF_COMBINE_PUBLIC=2,
    OMF_SEGDEF_COMBINE_PUBLIC4=4,
    OMF_SEGDEF_COMBINE_STACK=5,
    OMF_SEGDEF_COMBINE_COMMON=6,
    OMF_SEGDEF_COMBINE_PUBLIC7=7
};

enum {
    OMF_FIXUPP_LOCATION_LOBYTE=0,                   // low byte (16-bit offset)
    OMF_FIXUPP_LOCATION_16BIT_OFFSET=1,             // 16-bit offset
    OMF_FIXUPP_LOCATION_16BIT_SEGMENT_BASE=2,       // 16-bit segment base
    OMF_FIXUPP_LOCATION_16BIT_SEGMENT_OFFSET=3,     // 16:16 far pointer seg:off
    OMF_FIXUPP_LOCATION_HIBYTE=4,                   // high byte (16-bit offset)
    OMF_FIXUPP_LOCATION_16BIT_OFFSET_RESOLVED=5,    // basically same as 16BIT_OFFSET

    OMF_FIXUPP_LOCATION_32BIT_OFFSET=9,             // 32-bit offset

    OMF_FIXUPP_LOCATION_32BIT_SEGMENT_OFFSET=11,    // 16:32 far pointer seg:off

    OMF_FIXUPP_LOCATION_32BIT_OFFSET_RESOLVED=13    // basically same as 32BIT_OFFSET
};

enum {
    OMF_FIXUPP_FRAME_METHOD_SEGDEF=0,
    OMF_FIXUPP_FRAME_METHOD_GRPDEF=1,
    OMF_FIXUPP_FRAME_METHOD_EXTDEF=2,

    OMF_FIXUPP_FRAME_METHOD_PREV_LEDATA=4,          // frame is determined by segmenet index of previous LEDATA
    OMF_FIXUPP_FRAME_METHOD_TARGET=5                // frame is target segment/group/extdef
};

enum {
    OMF_FIXUPP_TARGET_METHOD_SEGDEF=0,
    OMF_FIXUPP_TARGET_METHOD_GRPDEF=1,
    OMF_FIXUPP_TARGET_METHOD_EXTDEF=2
};

#define OMF_RECTYPE_THEADR      (0x80)

#define OMF_RECTYPE_EXTDEF      (0x8C)

#define OMF_RECTYPE_PUBDEF      (0x90)
#define OMF_RECTYPE_PUBDEF32    (0x91)

#define OMF_RECTYPE_LNAMES      (0x96)

#define OMF_RECTYPE_SEGDEF      (0x98)
#define OMF_RECTYPE_SEGDEF32    (0x99)

#define OMF_RECTYPE_GRPDEF      (0x9A)
#define OMF_RECTYPE_GRPDEF32    (0x9B)

#define OMF_RECTYPE_FIXUPP      (0x9C)
#define OMF_RECTYPE_FIXUPP32    (0x9D)

#define OMF_RECTYPE_LEDATA      (0xA0)
#define OMF_RECTYPE_LEDATA32    (0xA1)

#define OMF_RECTYPE_LIDATA      (0xA2)
#define OMF_RECTYPE_LIDATA32    (0xA3)

#define OMF_RECTYPE_LEXTDEF     (0xB4)
#define OMF_RECTYPE_LEXTDEF32   (0xB5)

#define OMF_RECTYPE_LPUBDEF     (0xB6)
#define OMF_RECTYPE_LPUBDEF32   (0xB7)

extern char                             omf_temp_str[255+1/*NUL*/];

struct omf_record_t {
    unsigned char           rectype;
    unsigned short          reclen;             // amount of data in data, reclen < data_alloc not including checksum
    unsigned short          recpos;             // read/write position
    unsigned char*          data;               // data if != NULL. checksum is at data[reclen]
    size_t                  data_alloc;         // amount of data allocated if data != NULL or amount TO alloc if data == NULL

    unsigned long           rec_file_offset;    // file offset of record (~0UL if undefined)
};

// this is filled in by a utility function after reading the OMF record from the beginning.
// the data pointer is valid UNTIL the OMF record is overwritten/rewritten, so take the
// data right after parsing the header, before you read another OMF record.
struct omf_ledata_info_t {
    unsigned int                        segment_index;
    unsigned long                       enum_data_offset;
    unsigned long                       data_length;
    unsigned char*                      data;
};

struct omf_fixupp_t {
    unsigned int                        segment_relative:1; // M bit [1=segment relative 0=self relative]
    unsigned int                        location:4;         // location
    unsigned int                        frame_method:3;     // frame method
    unsigned int                        target_method:3;    // target method
    unsigned int                        _pad_:5;            // [pad]
    uint16_t                            data_record_offset; // offset in last LEDATA (relative to LEDATA's enum offset)
    uint16_t                            frame_index;        // frame index (SEGDEF, GRPDEF, etc. according to frame method)
    uint16_t                            target_index;       // target index (SEGDEF, GRPDEF, etc. according to target method)
    unsigned long                       target_displacement;
    unsigned long                       omf_rec_file_offset;// file offset of LEDATA record
    unsigned long                       omf_rec_file_enoffs;
    unsigned char                       omf_rec_file_header;
};

struct omf_fixupp_thread_t {
    unsigned int                        method:3;           // current method
    unsigned int                        alloc:1;            // allocated
    unsigned int                        _pad_:4;
    unsigned int                        index;              // index (if method 0/1/2)
};

struct omf_fixupps_context_t {
    struct omf_fixupp_thread_t          frame_thread[4];    // frame "thread"s
    struct omf_fixupp_thread_t          target_thread[4];   // target "thread"s

    struct omf_fixupp_t*                omf_FIXUPPS;
    unsigned int                        omf_FIXUPPS_count;
    unsigned int                        omf_FIXUPPS_alloc;
};

struct omf_pubdef_t {
    char*                           name_string;
    unsigned int                    group_index;
    unsigned int                    segment_index;
    unsigned long                   public_offset;
    unsigned int                    type_index;
    unsigned char                   type;
};

struct omf_pubdefs_context_t {
    struct omf_pubdef_t*            omf_PUBDEFS;
    unsigned int                    omf_PUBDEFS_count;
    unsigned int                    omf_PUBDEFS_alloc;
};

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

struct omf_extdef_t {
    char*                           name_string;
    unsigned int                    type_index;
    unsigned char                   type;
};

struct omf_extdefs_context_t {
    struct omf_extdef_t*            omf_EXTDEFS;
    unsigned int                    omf_EXTDEFS_count;
    unsigned int                    omf_EXTDEFS_alloc;
};

// grpdefs context:
//
//    segdefs = [ 2 6 3 5 4 1 2 4 ]
//               |   | |     |
//               |   | |     |
//    GRPDEFs = [|   | |     |
//             0 +   | |     |
//             2 ----+ |     |
//             3 ------+     |
//             6 ------------+
//             ]
//
//    where a GRPDEF entry is:
//      starting index into segdefs (0-based)
//      number of entries.

struct omf_grpdef_t {
    unsigned int                        group_name_index;
    unsigned int                        index;
    unsigned int                        count;
};

struct omf_grpdefs_context_t {
    uint16_t*                           segdefs;
    unsigned int                        segdefs_count;
    unsigned int                        segdefs_alloc;

    struct omf_grpdef_t*                omf_GRPDEFS;
    unsigned int                        omf_GRPDEFS_count;
    unsigned int                        omf_GRPDEFS_alloc;
};

/* LNAMES collection */
struct omf_lnames_context_t {
    char**              omf_LNAMES;
    unsigned int        omf_LNAMES_count;
    unsigned int        omf_LNAMES_alloc;
};

struct omf_context_t {
    const char*                         last_error;
    struct omf_lnames_context_t         LNAMEs;
    struct omf_segdefs_context_t        SEGDEFs;
    struct omf_grpdefs_context_t        GRPDEFs;
    struct omf_extdefs_context_t        EXTDEFs;
    struct omf_pubdefs_context_t        PUBDEFs;
    struct omf_fixupps_context_t        FIXUPPs;
    struct omf_record_t                 record; // reading, during parsing
    unsigned short                      library_block_size;// is .LIB archive if nonzero
    unsigned short                      last_LEDATA_seg;
    unsigned long                       last_LEDATA_rec;
    unsigned long                       last_LEDATA_eno;
    unsigned char                       last_LEDATA_hdr;
    char*                               THEADR;
    struct {
        unsigned int                    verbose:1;
    } flags;
};

void omf_extdefs_context_init_extdef(struct omf_extdef_t * const ctx);
void omf_extdefs_context_init(struct omf_extdefs_context_t * const ctx);
void omf_extdefs_context_free_entries(struct omf_extdefs_context_t * const ctx);
void omf_extdefs_context_free(struct omf_extdefs_context_t * const ctx);
struct omf_extdefs_context_t *omf_extdefs_context_create(void);
struct omf_extdefs_context_t *omf_extdefs_context_destroy(struct omf_extdefs_context_t * const ctx);
int omf_extdefs_context_alloc_extdefs(struct omf_extdefs_context_t * const ctx);
struct omf_extdef_t *omf_extdefs_context_add_extdef(struct omf_extdefs_context_t * const ctx);
const struct omf_extdef_t *omf_extdefs_context_get_extdef(const struct omf_extdefs_context_t * const ctx,unsigned int i);
int omf_extdefs_context_set_extdef_name(struct omf_extdefs_context_t * const ctx,struct omf_extdef_t * const extdef,const char * const name,const size_t namelen);
const char *omf_extdef_type_to_string(const unsigned char t);

// return the lowest valid LNAME index
static inline unsigned int omf_extdefs_context_get_lowest_index(const struct omf_extdefs_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_extdefs_context_get_highest_index(const struct omf_extdefs_context_t * const ctx) {
    return ctx->omf_EXTDEFS_count;
}

// return the index the next LNAME will get after calling add_extdef()
static inline unsigned int omf_extdefs_context_get_next_add_index(const struct omf_extdefs_context_t * const ctx) {
    return ctx->omf_EXTDEFS_count + 1;
}

void omf_fixupps_clear_thread(struct omf_fixupp_thread_t * const th);
void omf_fixupps_clear_threads(struct omf_fixupps_context_t * const ctx);
void omf_fixupps_context_init(struct omf_fixupps_context_t * const ctx);
int omf_fixupps_context_alloc_fixupps(struct omf_fixupps_context_t * const ctx);
void omf_fixupps_context_free_entries(struct omf_fixupps_context_t * const ctx);
void omf_fixupps_context_free(struct omf_fixupps_context_t * const ctx);
struct omf_fixupps_context_t *omf_fixupps_context_create(void);
struct omf_fixupps_context_t *omf_fixupps_context_destroy(struct omf_fixupps_context_t * const ctx);
struct omf_fixupp_t *omf_fixupps_context_add_fixupp(struct omf_fixupps_context_t * const ctx);
const struct omf_fixupp_t *omf_fixupps_context_get_fixupp(const struct omf_fixupps_context_t * const ctx,unsigned int i);
const char *omf_fixupp_location_to_str(const unsigned char loc);
const char *omf_fixupp_frame_method_to_str(const unsigned char m);
const char *omf_fixupp_target_method_to_str(const unsigned char m);

// return the lowest valid LNAME index
static inline unsigned int omf_fixupps_context_get_lowest_index(const struct omf_fixupps_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_fixupps_context_get_highest_index(const struct omf_fixupps_context_t * const ctx) {
    return ctx->omf_FIXUPPS_count;
}

// return the index the next LNAME will get after calling add_segdef()
static inline unsigned int omf_fixupps_context_get_next_add_index(const struct omf_fixupps_context_t * const ctx) {
    return ctx->omf_FIXUPPS_count + 1;
}

void omf_grpdefs_context_init(struct omf_grpdefs_context_t * const ctx);
int omf_grpdefs_context_alloc_grpdefs(struct omf_grpdefs_context_t * const ctx);
void omf_grpdefs_context_free_entries(struct omf_grpdefs_context_t * const ctx);
void omf_grpdefs_context_free(struct omf_grpdefs_context_t * const ctx);
struct omf_grpdefs_context_t *omf_grpdefs_context_create(void);
struct omf_grpdefs_context_t *omf_grpdefs_context_destroy(struct omf_grpdefs_context_t * const ctx);
int omf_grpdefs_context_get_grpdef_segdef(const struct omf_grpdefs_context_t * const ctx,const struct omf_grpdef_t *grp,const unsigned int i);
int omf_grpdefs_context_add_grpdef_segdef(struct omf_grpdefs_context_t * const ctx,struct omf_grpdef_t *grp,unsigned int segdef);
struct omf_grpdef_t *omf_grpdefs_context_add_grpdef(struct omf_grpdefs_context_t * const ctx);
const struct omf_grpdef_t *omf_grpdefs_context_get_grpdef(const struct omf_grpdefs_context_t * const ctx,unsigned int i);

// return the lowest valid LNAME index
static inline unsigned int omf_grpdefs_context_get_lowest_index(const struct omf_grpdefs_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_grpdefs_context_get_highest_index(const struct omf_grpdefs_context_t * const ctx) {
    return ctx->omf_GRPDEFS_count;
}

// return the index the next LNAME will get after calling add_segdef()
static inline unsigned int omf_grpdefs_context_get_next_add_index(const struct omf_grpdefs_context_t * const ctx) {
    return ctx->omf_GRPDEFS_count + 1;
}

void omf_lnames_context_init(struct omf_lnames_context_t * const ctx);
int omf_lnames_context_alloc_names(struct omf_lnames_context_t * const ctx);
int omf_lnames_context_clear_name(struct omf_lnames_context_t * const ctx,unsigned int i);
const char *omf_lnames_context_get_name(const struct omf_lnames_context_t * const ctx,unsigned int i);
const char *omf_lnames_context_get_name_safe(const struct omf_lnames_context_t * const ctx,unsigned int i);
int omf_lnames_context_set_name(struct omf_lnames_context_t * const ctx,unsigned int i,const char *name,const unsigned char namelen);
int omf_lnames_context_add_name(struct omf_lnames_context_t * const ctx,const char *name,const unsigned char namelen);
void omf_lnames_context_clear_names(struct omf_lnames_context_t * const ctx);
void omf_lnames_context_free_names(struct omf_lnames_context_t * const ctx);
void omf_lnames_context_free(struct omf_lnames_context_t * const ctx);
struct omf_lnames_context_t *omf_lnames_context_create(void);
struct omf_lnames_context_t *omf_lnames_context_destroy(struct omf_lnames_context_t * const ctx);

// return the lowest valid LNAME index
static inline unsigned int omf_lnames_context_get_lowest_index(const struct omf_lnames_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_lnames_context_get_highest_index(const struct omf_lnames_context_t * const ctx) {
    return ctx->omf_LNAMES_count;
}

// return the index the next LNAME will get after calling add_name()
static inline unsigned int omf_lnames_context_get_next_add_index(const struct omf_lnames_context_t * const ctx) {
    return ctx->omf_LNAMES_count + 1;
}

int omf_context_read_fd(struct omf_context_t * const ctx,int fd);
int omf_context_next_lib_module_fd(struct omf_context_t * const ctx,int fd);

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_grpdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);

int omf_ledata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec);
unsigned char omf_record_is_modend(const struct omf_record_t * const rec);
void omf_record_init(struct omf_record_t * const rec);
void omf_record_data_free(struct omf_record_t * const rec);
int omf_record_data_alloc(struct omf_record_t * const rec,size_t sz);
void omf_record_clear(struct omf_record_t * const rec);
unsigned short omf_record_lseek(struct omf_record_t * const rec,unsigned short pos);
size_t omf_record_can_write(const struct omf_record_t * const rec);
size_t omf_record_data_available(const struct omf_record_t * const rec);
unsigned char omf_record_get_byte_fast(struct omf_record_t * const rec);
unsigned char omf_record_get_byte(struct omf_record_t * const rec);
unsigned short omf_record_get_word_fast(struct omf_record_t * const rec);
unsigned short omf_record_get_word(struct omf_record_t * const rec);
unsigned long omf_record_get_dword_fast(struct omf_record_t * const rec);
unsigned long omf_record_get_dword(struct omf_record_t * const rec);
unsigned int omf_record_get_index(struct omf_record_t * const rec);
int omf_record_read_data(unsigned char *dst,unsigned int len,struct omf_record_t *rec);
int omf_record_get_lenstr(char *dst,const size_t dstmax,struct omf_record_t *rec);
void omf_record_free(struct omf_record_t * const rec);
const char *omf_rectype_to_str_long(unsigned char rt);
const char *omf_rectype_to_str(unsigned char rt);

static inline unsigned char omf_record_eof(const struct omf_record_t * const rec) {
    return (omf_record_data_available(rec) == 0);
}

// LIDATA has same header, but data points to blocks
static inline int omf_lidata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    return omf_ledata_parse_header(info,rec);
}

void omf_pubdefs_context_init_pubdef(struct omf_pubdef_t * const ctx);
void omf_pubdefs_context_init(struct omf_pubdefs_context_t * const ctx);
void omf_pubdefs_context_free_entries(struct omf_pubdefs_context_t * const ctx);
void omf_pubdefs_context_free(struct omf_pubdefs_context_t * const ctx);
struct omf_pubdefs_context_t *omf_pubdefs_context_create(void);
struct omf_pubdefs_context_t *omf_pubdefs_context_destroy(struct omf_pubdefs_context_t * const ctx);
int omf_pubdefs_context_alloc_pubdefs(struct omf_pubdefs_context_t * const ctx);
struct omf_pubdef_t *omf_pubdefs_context_add_pubdef(struct omf_pubdefs_context_t * const ctx);
const struct omf_pubdef_t *omf_pubdefs_context_get_pubdef(const struct omf_pubdefs_context_t * const ctx,unsigned int i);
int omf_pubdefs_context_set_pubdef_name(struct omf_pubdefs_context_t * const ctx,struct omf_pubdef_t * const pubdef,const char * const name,const size_t namelen);
const char *omf_pubdef_type_to_string(const unsigned char t);

// return the lowest valid LNAME index
static inline unsigned int omf_pubdefs_context_get_lowest_index(const struct omf_pubdefs_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_pubdefs_context_get_highest_index(const struct omf_pubdefs_context_t * const ctx) {
    return ctx->omf_PUBDEFS_count;
}

// return the index the next LNAME will get after calling add_pubdef()
static inline unsigned int omf_pubdefs_context_get_next_add_index(const struct omf_pubdefs_context_t * const ctx) {
    return ctx->omf_PUBDEFS_count + 1;
}

void omf_segdefs_context_init_segdef(struct omf_segdef_t *s);
void omf_segdefs_context_init(struct omf_segdefs_context_t * const ctx);
int omf_segdefs_context_alloc_segdefs(struct omf_segdefs_context_t * const ctx);
void omf_segdefs_context_free_entries(struct omf_segdefs_context_t * const ctx);
void omf_segdefs_context_free(struct omf_segdefs_context_t * const ctx);
struct omf_segdefs_context_t *omf_segdefs_context_create(void);
struct omf_segdefs_context_t *omf_segdefs_context_destroy(struct omf_segdefs_context_t * const ctx);
struct omf_segdef_t *omf_segdefs_context_add_segdef(struct omf_segdefs_context_t * const ctx);
const struct omf_segdef_t *omf_segdefs_context_get_segdef(const struct omf_segdefs_context_t * const ctx,unsigned int i);
const char *omf_segdefs_alignment_to_str(const unsigned char a);
const char *omf_segdefs_combination_to_str(const unsigned char c);

// return the lowest valid LNAME index
static inline unsigned int omf_segdefs_context_get_lowest_index(const struct omf_segdefs_context_t * const ctx) {
    (void)ctx;
    return 1;
}

// return the highest valid LNAME index
static inline unsigned int omf_segdefs_context_get_highest_index(const struct omf_segdefs_context_t * const ctx) {
    return ctx->omf_SEGDEFS_count;
}

// return the index the next LNAME will get after calling add_segdef()
static inline unsigned int omf_segdefs_context_get_next_add_index(const struct omf_segdefs_context_t * const ctx) {
    return ctx->omf_SEGDEFS_count + 1;
}

int omf_context_parse_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);
int omf_context_parse_LIDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);
int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec);

void omf_context_init(struct omf_context_t * const ctx);
void omf_context_free(struct omf_context_t * const ctx);
struct omf_context_t *omf_context_create(void);
struct omf_context_t *omf_context_destroy(struct omf_context_t * const ctx);
void omf_context_begin_file(struct omf_context_t * const ctx);
void omf_context_begin_module(struct omf_context_t * const ctx);
void omf_context_clear_for_module(struct omf_context_t * const ctx);
void omf_context_clear(struct omf_context_t * const ctx);
int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_SEGDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_GRPDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_EXTDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_PUBDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_FIXUPP_subrecord(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_FIXUPP(struct omf_context_t * const ctx,struct omf_record_t * const rec);

void dump_LIDATA(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info,const struct omf_record_t * const rec);
void dump_FIXUPP_entry(FILE *fp,const struct omf_context_t * const ctx,const struct omf_fixupp_t * const ent);
void dump_LEDATA(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info);
void dump_FIXUPP(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_PUBDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_EXTDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_SEGDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_GRPDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_LNAMES(FILE *fp,const struct omf_context_t * const ctx,unsigned int i);
void dump_THEADR(FILE *fp,const struct omf_context_t * const ctx);

#endif //_DOSLIB_OMF_OMFCTX_H

