
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
    OMF_EXTDEF_TYPE_GLOBAL=0,
    OMF_EXTDEF_TYPE_LOCAL
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

