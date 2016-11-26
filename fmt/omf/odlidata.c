
#include <fmt/omf/omf.h>

void dump_LIDATA(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info,const struct omf_record_t * const rec) {
    (void)rec;

    fprintf(fp,"LIDATA segment=\"%s\"(%u) data_offset=0x%lX(%lu) clength=0x%lX(%lu)\n",
        omf_context_get_segdef_name_safe(ctx,info->segment_index),
        info->segment_index,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);
}

