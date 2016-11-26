
#include <fmt/omf/osegdefs.h>

void omf_segdefs_context_init_segdef(struct omf_segdef_t *s) {
    memset(s,0,sizeof(*s));
}

void omf_segdefs_context_init(struct omf_segdefs_context_t * const ctx) {
    ctx->omf_SEGDEFS = NULL;
    ctx->omf_SEGDEFS_count = 0;
#if defined(LINUX)
    ctx->omf_SEGDEFS_alloc = 32768;
#else
    ctx->omf_SEGDEFS_alloc = 256;
#endif
}

int omf_segdefs_context_alloc_segdefs(struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS != NULL)
        return 0;

    if (ctx->omf_SEGDEFS_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_SEGDEFS_count = 0;
    ctx->omf_SEGDEFS = (struct omf_segdef_t*)malloc(sizeof(struct omf_segdef_t) * ctx->omf_SEGDEFS_alloc);
    if (ctx->omf_SEGDEFS == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

void omf_segdefs_context_free_entries(struct omf_segdefs_context_t * const ctx) {
    if (ctx->omf_SEGDEFS) {
        free(ctx->omf_SEGDEFS);
        ctx->omf_SEGDEFS = NULL;
    }
    ctx->omf_SEGDEFS_count = 0;
}

void omf_segdefs_context_free(struct omf_segdefs_context_t * const ctx) {
    omf_segdefs_context_free_entries(ctx);
}

struct omf_segdefs_context_t *omf_segdefs_context_create(void) {
    struct omf_segdefs_context_t *ctx;

    ctx = (struct omf_segdefs_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_segdefs_context_init(ctx);
    return ctx;
}

struct omf_segdefs_context_t *omf_segdefs_context_destroy(struct omf_segdefs_context_t * const ctx) {
    if (ctx != NULL) {
        omf_segdefs_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

struct omf_segdef_t *omf_segdefs_context_add_segdef(struct omf_segdefs_context_t * const ctx) {
    struct omf_segdef_t *seg;

    if (omf_segdefs_context_alloc_segdefs(ctx) < 0)
        return NULL;

    if (ctx->omf_SEGDEFS_count >= ctx->omf_SEGDEFS_alloc) {
        errno = ERANGE;
        return NULL;
    }

    seg = ctx->omf_SEGDEFS + ctx->omf_SEGDEFS_count;
    ctx->omf_SEGDEFS_count++;

    omf_segdefs_context_init_segdef(seg);
    return seg;
}

const struct omf_segdef_t *omf_segdefs_context_get_segdef(const struct omf_segdefs_context_t * const ctx,unsigned int i) {
    if (ctx->omf_SEGDEFS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_SEGDEFS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_SEGDEFS + i;
}

