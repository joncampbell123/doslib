
#include <fmt/omf/omf.h>

void dump_GRPDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
    unsigned int j;

    fprintf(fp,"GRPDEF (%u):",i);
    if (grpdef != NULL) {
        fprintf(fp," \"%s\"(%u)\n",
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,grpdef->group_name_index),
            grpdef->group_name_index);

        fprintf(fp,"    Contains: ");
        for (j=0;j < grpdef->count;j++) {
            int segdef = omf_grpdefs_context_get_grpdef_segdef(&ctx->GRPDEFs,grpdef,j);

            if (segdef >= 0) {
                const struct omf_segdef_t *sg = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,segdef);

                if (sg != NULL) {
                    fprintf(fp,"\"%s\"(%u) ",
                        omf_lnames_context_get_name_safe(&ctx->LNAMEs,sg->segment_name_index),
                        segdef);
                }
                else {
                    fprintf(fp,"[invalid SEGDEF] ");
                }
            }
            else {
                fprintf(fp,"[invalid] ");
            }
        }
        fprintf(fp,"\n");
    }
    else {
        fprintf(fp," [invalid]\n");
    }
}

