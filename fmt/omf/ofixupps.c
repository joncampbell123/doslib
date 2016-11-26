
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

void omf_fixupps_clear_thread(struct omf_fixupp_thread_t * const th) {
    th->method = 0;
    th->alloc = 0;
    th->_pad_ = 0;
    th->index = 0;
}

void omf_fixupps_clear_threads(struct omf_fixupps_context_t * const ctx) {
    unsigned int i;

    for (i=0;i < 4;i++) {
        omf_fixupps_clear_thread(&ctx->frame_thread[i]);
        omf_fixupps_clear_thread(&ctx->target_thread[i]);
    }
}

void omf_fixupps_context_init(struct omf_fixupps_context_t * const ctx) {
    ctx->omf_FIXUPPS = NULL;
    ctx->omf_FIXUPPS_count = 0;
#if defined(LINUX) || TARGET_MSDOS == 32
    ctx->omf_FIXUPPS_alloc = 32768;
#elif defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
    ctx->omf_FIXUPPS_alloc = 2048;
#elif defined(__TINY__)
    ctx->omf_FIXUPPS_alloc = 128;
#else
    ctx->omf_FIXUPPS_alloc = 256;
#endif
    omf_fixupps_clear_threads(ctx);
}

int omf_fixupps_context_alloc_fixupps(struct omf_fixupps_context_t * const ctx) {
    if (ctx->omf_FIXUPPS != NULL)
        return 0;

    if (ctx->omf_FIXUPPS_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_FIXUPPS_count = 0;
    ctx->omf_FIXUPPS = (struct omf_fixupp_t*)malloc(sizeof(struct omf_fixupp_t) * ctx->omf_FIXUPPS_alloc);
    if (ctx->omf_FIXUPPS == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

void omf_fixupps_context_free_entries(struct omf_fixupps_context_t * const ctx) {
    if (ctx->omf_FIXUPPS) {
        free(ctx->omf_FIXUPPS);
        ctx->omf_FIXUPPS = NULL;
    }
    ctx->omf_FIXUPPS_count = 0;
    omf_fixupps_clear_threads(ctx);
}

void omf_fixupps_context_free(struct omf_fixupps_context_t * const ctx) {
    omf_fixupps_context_free_entries(ctx);
}

struct omf_fixupps_context_t *omf_fixupps_context_create(void) {
    struct omf_fixupps_context_t *ctx;

    ctx = (struct omf_fixupps_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_fixupps_context_init(ctx);
    return ctx;
}

struct omf_fixupps_context_t *omf_fixupps_context_destroy(struct omf_fixupps_context_t * const ctx) {
    if (ctx != NULL) {
        omf_fixupps_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

struct omf_fixupp_t *omf_fixupps_context_add_fixupp(struct omf_fixupps_context_t * const ctx) {
    struct omf_fixupp_t *grp;

    if (omf_fixupps_context_alloc_fixupps(ctx) < 0)
        return NULL;

    if (ctx->omf_FIXUPPS_count >= ctx->omf_FIXUPPS_alloc) {
        errno = ERANGE;
        return NULL;
    }

    grp = ctx->omf_FIXUPPS + ctx->omf_FIXUPPS_count;
    ctx->omf_FIXUPPS_count++;

    // initialize the record for entry
    memset(grp,0,sizeof(*grp));
    return grp;
}

const struct omf_fixupp_t *omf_fixupps_context_get_fixupp(const struct omf_fixupps_context_t * const ctx,unsigned int i) {
    if (ctx->omf_FIXUPPS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_FIXUPPS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_FIXUPPS + i;
}

