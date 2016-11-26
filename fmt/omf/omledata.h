
#ifndef _DOSLIB_OMF_OMLEDATA_H
#define _DOSLIB_OMF_OMLEDATA_H

#include <fmt/omf/omfrec.h>

// this is filled in by a utility function after reading the OMF record from the beginning.
// the data pointer is valid UNTIL the OMF record is overwritten/rewritten, so take the
// data right after parsing the header, before you read another OMF record.
struct omf_ledata_info_t {
    unsigned int                        segment_index;
    unsigned long                       enum_data_offset;
    unsigned long                       data_length;
    unsigned char*                      data;
};

int omf_ledata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec);

// LIDATA has same header, but data points to blocks
static inline int omf_lidata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    return omf_ledata_parse_header(info,rec);
}

#endif //_DOSLIB_OMF_OMLEDATA_H

