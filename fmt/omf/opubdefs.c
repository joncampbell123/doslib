
#include <fmt/omf/omfcstr.h>
#include <fmt/omf/opubdefs.h>

void omf_pubdefs_context_init_pubdef(struct omf_pubdef_t * const ctx) {
    ctx->group_index = 0;
    ctx->segment_index = 0;
    ctx->public_offset = 0;
    ctx->type = OMF_PUBDEF_TYPE_LOCAL;
    ctx->name_string = NULL;
    ctx->type_index = 0;
}

void omf_pubdefs_context_init(struct omf_pubdefs_context_t * const ctx) {
    ctx->omf_PUBDEFS = NULL;
    ctx->omf_PUBDEFS_count = 0;
#if defined(LINUX)
    ctx->omf_PUBDEFS_alloc = 32768;
#else
    ctx->omf_PUBDEFS_alloc = 2048;
#endif
}

void omf_pubdefs_context_free_entries(struct omf_pubdefs_context_t * const ctx) {
    unsigned int i;

    if (ctx->omf_PUBDEFS) {
        for (i=0;i < ctx->omf_PUBDEFS_count;i++)
            cstr_free(&(ctx->omf_PUBDEFS[i].name_string));

        free(ctx->omf_PUBDEFS);
        ctx->omf_PUBDEFS = NULL;
    }
    ctx->omf_PUBDEFS_count = 0;
}

void omf_pubdefs_context_free(struct omf_pubdefs_context_t * const ctx) {
    omf_pubdefs_context_free_entries(ctx);
}

struct omf_pubdefs_context_t *omf_pubdefs_context_create(void) {
    struct omf_pubdefs_context_t *ctx;

    ctx = (struct omf_pubdefs_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_pubdefs_context_init(ctx);
    return ctx;
}

struct omf_pubdefs_context_t *omf_pubdefs_context_destroy(struct omf_pubdefs_context_t * const ctx) {
    if (ctx != NULL) {
        omf_pubdefs_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

int omf_pubdefs_context_alloc_pubdefs(struct omf_pubdefs_context_t * const ctx) {
    if (ctx->omf_PUBDEFS != NULL)
        return 0;

    if (ctx->omf_PUBDEFS_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_PUBDEFS_count = 0;
    ctx->omf_PUBDEFS = (struct omf_pubdef_t*)malloc(sizeof(struct omf_pubdef_t) * ctx->omf_PUBDEFS_alloc);
    if (ctx->omf_PUBDEFS == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

struct omf_pubdef_t *omf_pubdefs_context_add_pubdef(struct omf_pubdefs_context_t * const ctx) {
    struct omf_pubdef_t *seg;

    if (omf_pubdefs_context_alloc_pubdefs(ctx) < 0)
        return NULL;

    if (ctx->omf_PUBDEFS_count >= ctx->omf_PUBDEFS_alloc) {
        errno = ERANGE;
        return NULL;
    }

    seg = ctx->omf_PUBDEFS + ctx->omf_PUBDEFS_count;
    ctx->omf_PUBDEFS_count++;

    omf_pubdefs_context_init_pubdef(seg);
    return seg;
}

const struct omf_pubdef_t *omf_pubdefs_context_get_pubdef(const struct omf_pubdefs_context_t * const ctx,unsigned int i) {
    if (ctx->omf_PUBDEFS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_PUBDEFS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_PUBDEFS + i;
}

int omf_pubdefs_context_set_pubdef_name(struct omf_pubdefs_context_t * const ctx,struct omf_pubdef_t * const pubdef,const char * const name,const size_t namelen) {
    (void)ctx;

    if (cstr_set_n(&pubdef->name_string,name,namelen) < 0)
        return -1;

    return 0;
}

