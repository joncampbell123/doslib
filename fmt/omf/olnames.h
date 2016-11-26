
#ifndef _DOSLIB_OMF_OLNAMES_H
#define _DOSLIB_OMF_OLNAMES_H

#include "omfrec.h"

/* LNAMES collection */
struct omf_lnames_context_t {
    char**              omf_LNAMES;
    unsigned int        omf_LNAMES_count;
    unsigned int        omf_LNAMES_alloc;
};

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

#endif //_DOSLIB_OMF_OLNAMES_H

