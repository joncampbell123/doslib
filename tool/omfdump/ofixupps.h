
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

