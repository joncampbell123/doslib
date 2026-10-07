
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

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

// COMDAT (initialized communal data) holds data like LEDATA does, and FIXUPPs that follow refer to it.
//   Flags (byte)                   [1:1] iterated (LIDATA style data)
//   Attributes (byte)              [3:0] allocation type, 0 = explicit (the record names the segment)
//   Align (byte)
//   Enumerated data offset (word or dword)
//   Type index (index)
//   Public base (only if explicit allocation): group index, segment index, frame number (word) if segment index == 0
//   Public name (LNAMES index)
//   Data
int omf_context_parse_COMDAT(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    unsigned char flags,attributes;

    info->segment_index = 0;
    info->data = NULL;
    info->data_length = 0;
    info->iterated = 0;

    if (omf_record_data_available(rec) < 3) return -1;
    flags = omf_record_get_byte(rec);
    attributes = omf_record_get_byte(rec);
    (void)omf_record_get_byte(rec); // align

    if (omf_record_eof(rec)) return -1;
    info->enum_data_offset = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);

    if (omf_record_eof(rec)) return -1;
    (void)omf_record_get_index(rec); // type index

    if ((attributes & 0x0F) == 0/*explicit allocation*/) {
        if (omf_record_eof(rec)) return -1;
        (void)omf_record_get_index(rec); // group index
        info->segment_index = omf_record_get_index(rec);
        if (info->segment_index == 0)
            (void)omf_record_get_word(rec); // frame number
    }

    if (omf_record_eof(rec)) return -1;
    (void)omf_record_get_index(rec); // public name

    // what's left in the record is the data
    info->iterated = (flags & 0x02) ? 1 : 0;
    info->data = rec->data + rec->recpos;
    info->data_length = omf_record_data_available(rec);

    omf_context_update_last_LEDATA(ctx,info,rec);
    return 0;
}

