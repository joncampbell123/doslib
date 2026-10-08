
#include <fmt/omf/omf.h>

// LIBHEAD: the page size that modules are aligned to (from the record length), and where the dictionary is
void dump_LIBHEAD(FILE *fp,const struct omf_context_t * const ctx) {
    fprintf(fp,"LIBHEAD page_size=%u dictionary_offset=0x%lX dictionary_blocks=%u flags=0x%02X%s\n",
        ctx->library_block_size,
        ctx->library_dict_offset,
        ctx->library_dict_blocks,
        ctx->library_flags,
        (ctx->library_flags & OMF_LIBHEAD_FLAG_CASE_SENSITIVE) ? " CASE-SENSITIVE" : "");
}

