
#include <fmt/omf/omf.h>

// VERNUM: the OMF version, a string such as "1.0.0"
void dump_VERNUM(FILE *fp,struct omf_record_t * const rec) {
    if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0)
        fprintf(fp,"VERNUM [record too short]\n");
    else
        fprintf(fp,"VERNUM \"%s\"\n",omf_temp_str);
}

// VENDEXT: vendor number (word), then data only that vendor knows how to read
void dump_VENDEXT(FILE *fp,struct omf_record_t * const rec) {
    struct omf_ledata_info_t info;

    if (omf_record_data_available(rec) < 2) {
        fprintf(fp,"VENDEXT [record too short]\n");
        return;
    }

    fprintf(fp,"VENDEXT vendor=%u\n",omf_record_get_word(rec));

    // hex dump the rest, as if it were LEDATA at offset 0
    memset(&info,0,sizeof(info));
    info.data = rec->data + rec->recpos;
    info.data_length = omf_record_data_available(rec);
    dump_LEDATA_bytes(fp,&info);
}

