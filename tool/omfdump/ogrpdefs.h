
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

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

