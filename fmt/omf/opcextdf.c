
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

// CEXTDEF refers to COMDAT symbols in other modules. Each one takes the next EXTDEF index.
int omf_context_parse_CEXTDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_extdefs_context_get_next_add_index(&ctx->EXTDEFs);
    const char *name;

    while (!omf_record_eof(rec)) {
        struct omf_extdef_t *extdef = omf_extdefs_context_add_extdef(&ctx->EXTDEFs);

        if (extdef == NULL)
            return -1;

        // the name is an LNAMES index, not a string
        name = omf_lnames_context_get_name(&ctx->LNAMEs,omf_record_get_index(rec));
        if (name == NULL)
            return -1;

        if (omf_extdefs_context_set_extdef_name(&ctx->EXTDEFs,extdef,name,strlen(name)) < 0)
            return -1;

        if (omf_record_eof(rec))
            return -1;

        extdef->type = OMF_EXTDEF_TYPE_GLOBAL;
        extdef->record_type = rec->rectype;
        extdef->type_index = omf_record_get_index(rec);
    }

    return first_entry;
}
