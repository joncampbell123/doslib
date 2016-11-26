
#include <fmt/omf/omfctxnm.h>

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    if (i > 0) {
        const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
        if (grpdef == NULL) return NULL;
        return omf_lnames_context_get_name(&ctx->LNAMEs,grpdef->group_name_index);
    }

    return ""; // group index == 0 means segment is not part of a group
}

const char *omf_context_get_grpdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_grpdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

const char *omf_context_get_segdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,i);
    if (segdef == NULL) return NULL;
    return omf_lnames_context_get_name(&ctx->LNAMEs,segdef->segment_name_index);
}

const char *omf_context_get_segdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_segdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

const char *omf_context_get_extdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_extdef_t *extdef = omf_extdefs_context_get_extdef(&ctx->EXTDEFs,i);
    if (extdef == NULL) return NULL;
    return extdef->name_string;
}

const char *omf_context_get_extdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_extdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

