
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

void omf_lnames_context_init(struct omf_lnames_context_t * const ctx) {
    ctx->omf_LNAMES = NULL;
    ctx->omf_LNAMES_count = 0;
#if defined(LINUX) || TARGET_MSDOS == 32
    ctx->omf_LNAMES_alloc = 32768;
#elif defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
    ctx->omf_LNAMES_alloc = 1024;
#elif defined(__TINY__)
    ctx->omf_LNAMES_alloc = 256;
#else
    ctx->omf_LNAMES_alloc = 512;
#endif
}

int omf_lnames_context_alloc_names(struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES != NULL)
        return 0;

    if (ctx->omf_LNAMES_alloc == 0) {
        errno = EINVAL;
        return -1;
    }

    ctx->omf_LNAMES_count = 0;
    ctx->omf_LNAMES = (char**)malloc(sizeof(char*) * ctx->omf_LNAMES_alloc);
    if (ctx->omf_LNAMES == NULL)
        return -1; /* malloc sets errno */

    return 0;
}

int omf_lnames_context_clear_name(struct omf_lnames_context_t * const ctx,unsigned int i) {
    if ((i--) == 0) {
        errno = ERANGE;
        return -1; /* LNAMEs are indexed 1-based. After this test, i is converted to 0-based index */
    }

    if (i >= ctx->omf_LNAMES_count) {
        errno = ERANGE;
        return -1; /* out of range */
    }

    if (ctx->omf_LNAMES == NULL)
        return 0; /* LNAMEs array not allocated */

    if (ctx->omf_LNAMES[i] != NULL) {
        free(ctx->omf_LNAMES[i]);
        ctx->omf_LNAMES[i] = NULL;
    }

    return 0;
}

const char *omf_lnames_context_get_name(const struct omf_lnames_context_t * const ctx,unsigned int i) {
    if ((i--) == 0) {
        errno = ERANGE;
        return NULL;
    }

    if (i >= ctx->omf_LNAMES_count) {
        errno = ERANGE;
        return NULL;
    }

    return ctx->omf_LNAMES[i];
}

const char *omf_lnames_context_get_name_safe(const struct omf_lnames_context_t * const ctx,unsigned int i) {
    const char *r = omf_lnames_context_get_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

int omf_lnames_context_set_name(struct omf_lnames_context_t * const ctx,unsigned int i,const char *name,const unsigned char namelen) {
    if (name == NULL) {
        errno = EFAULT;
        return -1;
    }

    if ((i--) == 0) {
        errno = ERANGE;
        return -1; /* LNAMEs are indexed 1-based. After this test, i is converted to 0-based index */
    }

    if (i >= ctx->omf_LNAMES_alloc) {
        errno = ERANGE;
        return -1;
    }

    if (ctx->omf_LNAMES == NULL) {
        if (omf_lnames_context_alloc_names(ctx) < 0)
            return -1; /* sets errno */
    }

    while (ctx->omf_LNAMES_count <= i)
        ctx->omf_LNAMES[ctx->omf_LNAMES_count++] = NULL;

    if (cstr_set_n(&ctx->omf_LNAMES[i],name,namelen) < 0)
        return -1;

    return 0;
}

int omf_lnames_context_add_name(struct omf_lnames_context_t * const ctx,const char *name,const unsigned char namelen) {
    unsigned int idx;

    if (ctx->omf_LNAMES != NULL)
        idx = ctx->omf_LNAMES_count + 1;
    else
        idx = 1;

    if (omf_lnames_context_set_name(ctx,idx,name,namelen) < 0)
        return -1; /* sets errno */

    return (int)idx;
}

void omf_lnames_context_clear_names(struct omf_lnames_context_t * const ctx) {
    char *p;

    while (ctx->omf_LNAMES_count > 0) {
        --ctx->omf_LNAMES_count;
        p = ctx->omf_LNAMES[ctx->omf_LNAMES_count];
        ctx->omf_LNAMES[ctx->omf_LNAMES_count] = NULL;
        if (p != NULL) free(p);
    }
}

void omf_lnames_context_free_names(struct omf_lnames_context_t * const ctx) {
    if (ctx->omf_LNAMES) {
        omf_lnames_context_clear_names(ctx);
        free(ctx->omf_LNAMES);
        ctx->omf_LNAMES = NULL;
    }
    ctx->omf_LNAMES_count = 0;
}

void omf_lnames_context_free(struct omf_lnames_context_t * const ctx) {
    omf_lnames_context_free_names(ctx);
}

struct omf_lnames_context_t *omf_lnames_context_create(void) {
    struct omf_lnames_context_t *ctx;

    ctx = (struct omf_lnames_context_t*)malloc(sizeof(*ctx));
    if (ctx != NULL) omf_lnames_context_init(ctx);
    return ctx;
}

struct omf_lnames_context_t *omf_lnames_context_destroy(struct omf_lnames_context_t * const ctx) {
    if (ctx != NULL) {
        omf_lnames_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

