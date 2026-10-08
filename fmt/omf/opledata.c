
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

static void omf_context_update_last_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,const struct omf_record_t * const rec) {
    ctx->last_LEDATA_eno = info->enum_data_offset;
    ctx->last_LEDATA_rec = rec->rec_file_offset;
    ctx->last_LEDATA_seg = info->segment_index;
    ctx->last_LEDATA_hdr = rec->recpos;
    ctx->last_LEDATA_type = rec->rectype;
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
//   Flags (byte)                   OMF_COMDAT_FLAG_*
//   Attributes (byte)              [7:4] selection criteria [3:0] allocation type, 0 = explicit (the record names the segment)
//   Align (byte)                   0 = the SEGDEF says, else same as SEGDEF alignment
//   Enumerated data offset (word or dword)
//   Type index (index)
//   Public base (only if explicit allocation): group index, segment index, frame number (word) if segment index == 0
//   Public name (LNAMES index)
//   Data
// comdat may be NULL if the caller does not need the COMDAT header fields.
int omf_context_parse_COMDAT(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_comdat_t * const comdat,struct omf_record_t * const rec) {
    struct omf_comdat_t tmp,*c = (comdat != NULL) ? comdat : &tmp;
    unsigned char attributes;

    memset(c,0,sizeof(*c));
    info->segment_index = 0;
    info->data = NULL;
    info->data_length = 0;
    info->iterated = 0;
    info->is32bit = (rec->rectype & 1) ? 1 : 0;

    if (omf_record_data_available(rec) < 3) return -1;
    c->flags = omf_record_get_byte(rec);
    attributes = omf_record_get_byte(rec);
    c->selection = attributes >> 4;
    c->allocation = attributes & 0x0F;
    c->align = omf_record_get_byte(rec);

    if (omf_record_eof(rec)) return -1;
    info->enum_data_offset = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);

    if (omf_record_eof(rec)) return -1;
    c->type_index = omf_record_get_index(rec);

    if (c->allocation == OMF_COMDAT_ALLOC_EXPLICIT) {
        if (omf_record_eof(rec)) return -1;
        c->group_index = omf_record_get_index(rec);
        info->segment_index = omf_record_get_index(rec);
        if (info->segment_index == 0) {
            if (omf_record_data_available(rec) < 2) return -1;
            c->frame_number = omf_record_get_word(rec);
        }
    }

    if (omf_record_eof(rec)) return -1;
    c->public_name_index = omf_record_get_index(rec);

    // what's left in the record is the data
    info->iterated = (c->flags & OMF_COMDAT_FLAG_ITERATED) ? 1 : 0;
    info->data = rec->data + rec->recpos;
    info->data_length = omf_record_data_available(rec);

    omf_context_update_last_LEDATA(ctx,info,rec);
    return 0;
}

