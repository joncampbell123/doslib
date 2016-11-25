
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "omfrec.h"

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

void omf_segdefs_context_init_segdef(struct omf_segdef_t *s);
void omf_segdefs_context_init(struct omf_segdefs_context_t * const ctx);
int omf_segdefs_context_alloc_segdefs(struct omf_segdefs_context_t * const ctx);
void omf_segdefs_context_free_entries(struct omf_segdefs_context_t * const ctx);
void omf_segdefs_context_free(struct omf_segdefs_context_t * const ctx);
struct omf_segdefs_context_t *omf_segdefs_context_create(void);
struct omf_segdefs_context_t *omf_segdefs_context_destroy(struct omf_segdefs_context_t * const ctx);
struct omf_segdef_t *omf_segdefs_context_add_segdef(struct omf_segdefs_context_t * const ctx);
const struct omf_segdef_t *omf_segdefs_context_get_segdef(const struct omf_segdefs_context_t * const ctx,unsigned int i);

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

