
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_GRPDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_grpdefs_context_get_next_add_index(&ctx->GRPDEFs);
    struct omf_grpdef_t *grpdef = omf_grpdefs_context_add_grpdef(&ctx->GRPDEFs);

    if (grpdef == NULL)
        return -1;

    if (omf_record_eof(rec))
        return -1;

    grpdef->group_name_index = omf_record_get_index(rec);

    while (!omf_record_eof(rec)) {
        unsigned char index = omf_record_get_byte(rec);
        unsigned int segdef = omf_record_get_index(rec);

        // we only support index 0xFF (segdef)
        if (index != 0xFF)
            return -1;

        if (omf_grpdefs_context_add_grpdef_segdef(&ctx->GRPDEFs,grpdef,segdef) < 0)
            return -1;
    }

    return first_entry;
}

