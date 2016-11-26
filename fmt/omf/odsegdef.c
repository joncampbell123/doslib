
#include <fmt/omf/omf.h>

void dump_SEGDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,i);

    fprintf(fp,"SEGDEF (%u):\n",i);
    if (segdef != NULL) {
        fprintf(fp,"    Alignment=%s(%u) combination=%s(%u) big=%u frame=%u offset=%u use%u\n",
            omf_segdefs_alignment_to_str(segdef->attr.f.f.alignment),
            segdef->attr.f.f.alignment,
            omf_segdefs_combination_to_str(segdef->attr.f.f.alignment),
            segdef->attr.f.f.combination,
            segdef->attr.f.f.big_segment,
            segdef->attr.f.f.use32?32U:16U,
            segdef->attr.frame_number,
            segdef->attr.offset);
        fprintf(fp,"    Length=%lu name=\"%s\"(%u) class=\"%s\"(%u) overlay=\"%s\"(%u)\n",
            (unsigned long)segdef->segment_length,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->segment_name_index),
            segdef->segment_name_index,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->class_name_index),
            segdef->class_name_index,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->overlay_name_index),
            segdef->overlay_name_index);
    }
}

