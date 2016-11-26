
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

void omf_grpdefs_context_init(struct omf_grpdefs_context_t * const ctx) {
    ctx->segdefs = NULL;
    ctx->segdefs_count = 0;
#if defined(LINUX) || TARGET_MSDOS == 32
    ctx->segdefs_alloc = 32768;
#elif defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
    ctx->segdefs_alloc = 1024;
#elif defined(__TINY__)
    ctx->segdefs_alloc = 32;
#else
    ctx->segdefs_alloc = 128;
#endif

    ctx->omf_GRPDEFS = NULL;
    ctx->omf_GRPDEFS_count = 0;
#if defined(LINUX) || TARGET_MSDOS == 32
    ctx->omf_GRPDEFS_alloc = 32768;
#elif defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
    ctx->omf_GRPDEFS_alloc = 128;
#elif defined(__TINY__)
    ctx->omf_GRPDEFS_alloc = 32;
#else
    ctx->omf_GRPDEFS_alloc = 64; // "Most linkers limit .. the total GRPDEFS to 31" well then we'll double it :)
#endif
}

int omf_grpdefs_context_alloc_grpdefs(struct omf_grpdefs_context_t * const ctx) {
    if (ctx->omf_GRPDEFS != NULL || ctx->segdefs != NULL)
        return 0;

    if (ctx->omf_GRPDEFS_alloc == 0 || ctx->segdefs_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_GRPDEFS_count = 0;
    ctx->omf_GRPDEFS = (struct omf_grpdef_t*)malloc(sizeof(struct omf_grpdef_t) * ctx->omf_GRPDEFS_alloc);
    if (ctx->omf_GRPDEFS == NULL)
        return -1; /* malloc sets errno */

    ctx->segdefs_count = 0;
    ctx->segdefs = (uint16_t*)malloc(sizeof(uint16_t) * ctx->segdefs_alloc);
    if (ctx->segdefs == NULL) {
        int x = errno;
        omf_grpdefs_context_free_entries(ctx); // may modify errno
        errno = x; // restore errno
        return -1; /* malloc sets errno */
    }

    return 0;
}

void omf_grpdefs_context_free_entries(struct omf_grpdefs_context_t * const ctx) {
    if (ctx->segdefs) {
        free(ctx->segdefs);
        ctx->segdefs = NULL;
    }
    ctx->segdefs_count = 0;

    if (ctx->omf_GRPDEFS) {
        free(ctx->omf_GRPDEFS);
        ctx->omf_GRPDEFS = NULL;
    }
    ctx->omf_GRPDEFS_count = 0;
}

void omf_grpdefs_context_free(struct omf_grpdefs_context_t * const ctx) {
    omf_grpdefs_context_free_entries(ctx);
}

struct omf_grpdefs_context_t *omf_grpdefs_context_create(void) {
    struct omf_grpdefs_context_t *ctx;

    ctx = (struct omf_grpdefs_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_grpdefs_context_init(ctx);
    return ctx;
}

struct omf_grpdefs_context_t *omf_grpdefs_context_destroy(struct omf_grpdefs_context_t * const ctx) {
    if (ctx != NULL) {
        omf_grpdefs_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

int omf_grpdefs_context_get_grpdef_segdef(const struct omf_grpdefs_context_t * const ctx,const struct omf_grpdef_t *grp,const unsigned int i) {
    if (ctx->segdefs == NULL) {
        errno = ERANGE;
        return -1;
    }
    if (i >= grp->count) {
        errno = ERANGE;
        return -1;
    }
    if ((grp->index+i) > ctx->segdefs_count) {
        errno = ERANGE;
        return -1;
    }

    return ctx->segdefs[grp->index+i];
}

int omf_grpdefs_context_add_grpdef_segdef(struct omf_grpdefs_context_t * const ctx,struct omf_grpdef_t *grp,unsigned int segdef) {
    if (segdef == 0) {
        errno = EINVAL;
        return -1;
    }
    if (ctx->segdefs == NULL) {
        errno = ENOSPC;
        return -1;
    }
    if (ctx->segdefs_count >= ctx->segdefs_alloc) {
        errno = ENOSPC;
        return -1;
    }
    // this structure only allows you to append to the last GRPDEF
    if (ctx->omf_GRPDEFS_count == 0 || grp != (ctx->omf_GRPDEFS + ctx->omf_GRPDEFS_count - 1)) {
        errno = EINVAL;
        return -1;
    }
    if (grp->index > ctx->segdefs_count || grp->count > ctx->segdefs_count ||
        (grp->index+grp->count) > ctx->segdefs_count) {
        errno = EINVAL;
        return -1;
    }

    {
        uint16_t *entry = ctx->segdefs + ctx->segdefs_count;

        *entry = segdef;
        ctx->segdefs_count++;
        grp->count++;
    }

    return 0;
}

struct omf_grpdef_t *omf_grpdefs_context_add_grpdef(struct omf_grpdefs_context_t * const ctx) {
    struct omf_grpdef_t *grp;

    if (omf_grpdefs_context_alloc_grpdefs(ctx) < 0)
        return NULL;

    if (ctx->omf_GRPDEFS_count >= ctx->omf_GRPDEFS_alloc ||
        ctx->segdefs_count >= ctx->segdefs_alloc) {
        errno = ERANGE;
        return NULL;
    }

    grp = ctx->omf_GRPDEFS + ctx->omf_GRPDEFS_count;
    ctx->omf_GRPDEFS_count++;

    // initialize the record for entry
    grp->group_name_index = 0;
    grp->index = ctx->segdefs_count;
    grp->count = 0;

    return grp;
}

const struct omf_grpdef_t *omf_grpdefs_context_get_grpdef(const struct omf_grpdefs_context_t * const ctx,unsigned int i) {
    if (ctx->omf_GRPDEFS == NULL) {
        errno = ERANGE;
        return NULL;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_GRPDEFS_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_GRPDEFS + i;
}

