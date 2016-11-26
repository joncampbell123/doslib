
#include <fmt/omf/opledata.h>

static void omf_context_update_last_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,const struct omf_record_t * const rec) {
    ctx->last_LEDATA_eno = info->enum_data_offset;
    ctx->last_LEDATA_rec = rec->rec_file_offset;
    ctx->last_LEDATA_seg = info->segment_index;
    ctx->last_LEDATA_hdr = rec->recpos;
}

int omf_context_parse_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    if (omf_ledata_parse_header(info,rec) < 0)
        return -1;

    omf_context_update_last_LEDATA(ctx,info,rec);
    return 0;
}

int omf_context_parse_LIDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    if (omf_lidata_parse_header(info,rec) < 0)
        return -1;

    omf_context_update_last_LEDATA(ctx,info,rec);
    return 0;
}

