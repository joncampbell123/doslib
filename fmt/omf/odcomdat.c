
#include <fmt/omf/omf.h>

void dump_COMDAT(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info,const struct omf_comdat_t * const comdat) {
    fprintf(fp,"COMDAT \"%s\"(%u) flags=0x%02X",
        omf_lnames_context_get_name_safe(&ctx->LNAMEs,comdat->public_name_index),
        comdat->public_name_index,
        comdat->flags);
    if (comdat->flags & OMF_COMDAT_FLAG_CONTINUATION)
        fprintf(fp," CONTINUATION");
    if (comdat->flags & OMF_COMDAT_FLAG_ITERATED)
        fprintf(fp," ITERATED");
    if (comdat->flags & OMF_COMDAT_FLAG_LOCAL)
        fprintf(fp," LOCAL");
    if (comdat->flags & OMF_COMDAT_FLAG_DATA_IN_CODE)
        fprintf(fp," DATA-IN-CODE");
    fprintf(fp,"\n");

    fprintf(fp,"    selection=%s(%u) allocation=%s(%u) align=%s(%u) typeindex=%u\n",
        omf_comdat_selection_to_str(comdat->selection),
        comdat->selection,
        omf_comdat_allocation_to_str(comdat->allocation),
        comdat->allocation,
        comdat->align != 0 ? omf_segdefs_alignment_to_str(comdat->align) : "FROM-SEGDEF",
        comdat->align,
        comdat->type_index);

    if (comdat->allocation == OMF_COMDAT_ALLOC_EXPLICIT) {
        fprintf(fp,"    group=\"%s\"(%u)",
            omf_context_get_grpdef_name_safe(ctx,comdat->group_index),
            comdat->group_index);

        if (info->segment_index == 0) {
            fprintf(fp," segment=ABSOLUTE frame=0x%04X",
                comdat->frame_number);
        }
        else {
            fprintf(fp," segment=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,info->segment_index),
                info->segment_index);
        }

        fprintf(fp,"\n");
    }

    fprintf(fp,"    data_offset=0x%lX(%lu) length=0x%lX(%lu)\n",
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);

    if (!info->iterated)
        dump_LEDATA_bytes(fp,info);
}

