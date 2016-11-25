
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

enum {
    OMF_PUBDEF_TYPE_GLOBAL=0,
    OMF_PUBDEF_TYPE_LOCAL
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

