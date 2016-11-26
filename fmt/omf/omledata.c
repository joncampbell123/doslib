
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_ledata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    info->data = NULL;
    info->data_length = 0;

    if (omf_record_eof(rec)) return -1;
    info->segment_index = omf_record_get_index(rec);

    if (omf_record_eof(rec)) return -1;
    info->enum_data_offset = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);

    // what's left in the record is the data
    info->data = rec->data + rec->recpos;
    info->data_length = omf_record_data_available(rec);

    // DONE
    return 0;
}

