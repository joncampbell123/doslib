
#include <fmt/omf/omf.h>

void dump_FIXUPP_entry(FILE *fp,const struct omf_context_t * const ctx,const struct omf_fixupp_t * const ent) {
    if (!ent->alloc)
        return;

    fprintf(fp,"    %s-relative location=%s(%u) frame_method=%s(%u) apply-to-seg='%s'(%u)",
            ent->segment_relative?"seg":"self",
            omf_fixupp_location_to_str(ent->location),
            ent->location,
            omf_fixupp_frame_method_to_str(ent->frame_method),
            ent->frame_method,
            omf_context_get_segdef_name_safe(ctx,ent->fixup_segdef_index),
            ent->fixup_segdef_index);

    if (ent->frame_method == 0/*SEGDEF*/) {
        fprintf(fp," frame_index=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }
    else if (ent->frame_method == 1/*GRPDEF*/) {
        fprintf(fp," frame_index=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }
    else if (ent->frame_method == 2/*EXTDEF*/) {
        fprintf(fp," frame_index=\"%s\"(%u)",
                omf_context_get_extdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }

    fprintf(fp,"\n");

    fprintf(fp,"    target_method=\"%s\"(%u)",
            omf_fixupp_target_method_to_str(ent->target_method),
            ent->target_method);

    if (ent->target_method == 0/*SEGDEF*/) {
        fprintf(fp," target_index=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }
    else if (ent->target_method == 1/*GRPDEF*/) {
        fprintf(fp," target_index=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }
    else if (ent->target_method == 2/*EXTDEF*/) {
        fprintf(fp," target_index=\"%s\"(%u)",
                omf_context_get_extdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }

    fprintf(fp," data_rec_ofs=0x%lX(%lu)\n",
            (unsigned long)ent->data_record_offset,
            (unsigned long)ent->data_record_offset);
    fprintf(fp,"    target_displacement=%lu ledata_rec_ofs=0x%lX(%lu)+%u absrecofs=0x%lX(%lu)\n",
            (unsigned long)ent->target_displacement,
            (unsigned long)ent->omf_rec_file_offset,
            (unsigned long)ent->omf_rec_file_offset,
            (unsigned int)ent->omf_rec_file_header,
            (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset,
            (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset);
}

void dump_FIXUPP(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    while (i <= omf_fixupps_context_get_highest_index(&ctx->FIXUPPs)) {
        const struct omf_fixupp_t *ent = omf_fixupps_context_get_fixupp(&ctx->FIXUPPs,i);

        fprintf(fp,"FIXUPP[%u]:\n",i);
        if (ent != NULL) dump_FIXUPP_entry(fp,ctx,ent);

        i++;
    }
}

