
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int len;

    len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
    if (len < 0) return -1;

    if (cstr_set_n(&ctx->THEADR,omf_temp_str,len) < 0)
        return -1;

    return 0;
}

