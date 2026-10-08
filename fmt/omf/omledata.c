
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_ledata_parse_header(struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    info->data = NULL;
    info->data_length = 0;
    info->iterated = 0;
    info->is32bit = (rec->rectype & 1) ? 1 : 0;

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

// length of the data of an LEDATA, LIDATA, or COMDAT record, with iterated data expanded.
// returns 0, or -1 if the iterated data blocks are not valid.
int omf_ledata_info_get_length(const struct omf_ledata_info_t * const info,unsigned long * const len) {
    if (info->iterated)
        return omf_lidata_expand(info,NULL,0,len);

    *len = info->data_length;
    return 0;
}

// copy up to dstmax bytes of the data of an LEDATA, LIDATA, or COMDAT record, with iterated data expanded.
// returns 0, or -1 if the iterated data blocks are not valid.
int omf_ledata_info_copy_data(unsigned char * const dst,const unsigned long dstmax,const struct omf_ledata_info_t * const info) {
    unsigned long len;

    if (info->iterated)
        return omf_lidata_expand(info,dst,dstmax,&len);

    len = (info->data_length < dstmax) ? info->data_length : dstmax;
    if (len != 0) memcpy(dst,info->data,len);
    return 0;
}

