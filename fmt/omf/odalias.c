
#include <fmt/omf/omf.h>

// ALIAS: pairs of names, an alias and the symbol to use for it
void dump_ALIAS(FILE *fp,struct omf_record_t * const rec) {
    fprintf(fp,"ALIAS:\n");

    while (!omf_record_eof(rec)) {
        if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0)
            break;
        fprintf(fp,"    \"%s\"",omf_temp_str);

        if (omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec) < 0) {
            fprintf(fp," [record too short]\n");
            return;
        }
        fprintf(fp," -> \"%s\"\n",omf_temp_str);
    }

    if (!omf_record_eof(rec))
        fprintf(fp,"    [record too short]\n");
}

