
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_lnames_context_get_next_add_index(&ctx->LNAMEs);
    int len;

    while (!omf_record_eof(rec)) {
        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_lnames_context_add_name(&ctx->LNAMEs,omf_temp_str,len) < 0)
            return -1;
    }

    return first_entry;
}

