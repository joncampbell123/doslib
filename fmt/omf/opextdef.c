
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_EXTDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_extdefs_context_get_next_add_index(&ctx->EXTDEFs);
    unsigned int type;
    int len;

    if ((rec->rectype & 0xFE) == OMF_RECTYPE_LEXTDEF/*0xB4*/)
        type = OMF_EXTDEF_TYPE_LOCAL;
    else
        type = OMF_EXTDEF_TYPE_GLOBAL;

    while (!omf_record_eof(rec)) {
        struct omf_extdef_t *extdef = omf_extdefs_context_add_extdef(&ctx->EXTDEFs);

        if (extdef == NULL)
            return -1;

        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_extdefs_context_set_extdef_name(&ctx->EXTDEFs,extdef,omf_temp_str,len) < 0)
            return -1;

        if (omf_record_eof(rec))
            return -1;

        extdef->type = type;
        extdef->type_index = omf_record_get_index(rec);
    }

    return first_entry;
}

