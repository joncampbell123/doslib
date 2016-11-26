
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_SEGDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_segdefs_context_get_next_add_index(&ctx->SEGDEFs);
    struct omf_segdef_t *segdef = omf_segdefs_context_add_segdef(&ctx->SEGDEFs);

    if (segdef == NULL)
        return -1;

    // read segment attributes
    //   alignment      [7:5]
    //   combination    [4:2]
    //   big (max)      [1:1]
    //   use32          [0:0]
    segdef->attr.f.raw = omf_record_get_byte(rec);
    // frame number (conditional)
    if (segdef->attr.f.f.alignment == 0) {
        segdef->attr.frame_number = omf_record_get_word(rec);
        segdef->attr.offset = omf_record_get_byte(rec);
    }

    if (omf_record_eof(rec))
        return -1;

    if (rec->rectype & 1/*32-bit version*/)
        segdef->segment_length = omf_record_get_dword(rec);
    else
        segdef->segment_length = omf_record_get_word(rec);

    segdef->segment_name_index = omf_record_get_index(rec);
    segdef->class_name_index = omf_record_get_index(rec);

    if (omf_record_eof(rec))
        return -1;

    segdef->overlay_name_index = omf_record_get_index(rec);
    return first_entry;
}

