
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_PUBDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_pubdefs_context_get_next_add_index(&ctx->PUBDEFs);
    unsigned int base_segment_index;
    unsigned int base_group_index;
    unsigned int type;
    int len;

    if ((rec->rectype & 0xFE) == OMF_RECTYPE_LPUBDEF/*0xB6*/)
        type = OMF_PUBDEF_TYPE_LOCAL;
    else
        type = OMF_PUBDEF_TYPE_GLOBAL;

    if (omf_record_eof(rec))
        return -1;

    base_group_index = omf_record_get_index(rec);
    base_segment_index = omf_record_get_index(rec);
    if (base_segment_index == 0) {
        (void)omf_record_get_word(rec); // base frame (ignored)
    }

    while (!omf_record_eof(rec)) {
        struct omf_pubdef_t *pubdef = omf_pubdefs_context_add_pubdef(&ctx->PUBDEFs);

        if (pubdef == NULL)
            return -1;

        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_pubdefs_context_set_pubdef_name(&ctx->PUBDEFs,pubdef,omf_temp_str,len) < 0)
            return -1;

        if (omf_record_eof(rec))
            return -1;

        pubdef->group_index = base_group_index;
        pubdef->segment_index = base_segment_index;
        pubdef->public_offset = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        pubdef->type_index = omf_record_get_index(rec);
        pubdef->type = type;
    }

    return first_entry;
}

