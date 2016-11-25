
#include "omfcstr.h"
#include "oextdefs.h"

void omf_extdefs_context_init_extdef(struct omf_extdef_t * const ctx) {
    ctx->type = OMF_EXTDEF_TYPE_LOCAL;
    ctx->name_string = NULL;
    ctx->type_index = 0;
}

void omf_extdefs_context_init(struct omf_extdefs_context_t * const ctx) {
    ctx->omf_EXTDEFS = NULL;
    ctx->omf_EXTDEFS_count = 0;
#if defined(LINUX)
    ctx->omf_EXTDEFS_alloc = 32768;
#else
    ctx->omf_EXTDEFS_alloc = 2048;
#endif
}

void omf_extdefs_context_free_entries(struct omf_extdefs_context_t * const ctx) {
    unsigned int i;

    if (ctx->omf_EXTDEFS) {
        for (i=0;i < ctx->omf_EXTDEFS_count;i++)
            cstr_free(&(ctx->omf_EXTDEFS[i].name_string));

        free(ctx->omf_EXTDEFS);
        ctx->omf_EXTDEFS = NULL;
    }
    ctx->omf_EXTDEFS_count = 0;
}

void omf_extdefs_context_free(struct omf_extdefs_context_t * const ctx) {
    omf_extdefs_context_free_entries(ctx);
}

struct omf_extdefs_context_t *omf_extdefs_context_create(void) {
    struct omf_extdefs_context_t *ctx;

    ctx = (struct omf_extdefs_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_extdefs_context_init(ctx);
    return ctx;
}

struct omf_extdefs_context_t *omf_extdefs_context_destroy(struct omf_extdefs_context_t * const ctx) {
    if (ctx != NULL) {
        omf_extdefs_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

int omf_extdefs_context_alloc_extdefs(struct omf_extdefs_context_t * const ctx) {
    if (ctx->omf_EXTDEFS != NULL)
        return 0;

    if (ctx->omf_EXTDEFS_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_EXTDEFS_count = 0;
    ctx->omf_EXTDEFS = (struct omf_extdef_t*)malloc(sizeof(struct omf_extdef_t) * ctx->omf_EXTDEFS_alloc);
    if (ctx->omf_EXTDEFS == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

struct omf_extdef_t *omf_extdefs_context_add_extdef(struct omf_extdefs_context_t * const ctx) {
    struct omf_extdef_t *seg;

    if (omf_extdefs_context_alloc_extdefs(ctx) < 0)
        return NULL;

    if (ctx->omf_EXTDEFS_count >= ctx->omf_EXTDEFS_alloc) {
        errno = ERANGE;
        return NULL;
    }

    seg = ctx->omf_EXTDEFS + ctx->omf_EXTDEFS_count;
    ctx->omf_EXTDEFS_count++;

    omf_extdefs_context_init_extdef(seg);
    return seg;
}

const struct omf_extdef_t *omf_extdefs_context_get_extdef(const struct omf_extdefs_context_t * const ctx,unsigned int i) {
    if (ctx->omf_EXTDEFS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_EXTDEFS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_EXTDEFS + i;
}

int omf_extdefs_context_set_extdef_name(struct omf_extdefs_context_t * const ctx,struct omf_extdef_t * const extdef,const char * const name,const size_t namelen) {
    (void)ctx;

    if (cstr_set_n(&extdef->name_string,name,namelen) < 0)
        return -1;

    return 0;
}

