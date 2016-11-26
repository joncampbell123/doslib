
#include <fmt/omf/omf.h>

void dump_PUBDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    fprintf(fp,"PUBDEF (%u):\n",i);

    while (i <= omf_pubdefs_context_get_highest_index(&ctx->PUBDEFs)) {
        const struct omf_pubdef_t *pubdef = omf_pubdefs_context_get_pubdef(&ctx->PUBDEFs,i);

        fprintf(fp,"    [%u] ",i);
        if (pubdef != NULL) {
            if (pubdef->name_string != NULL)
                fprintf(fp,"\"%s\"",pubdef->name_string);

            fprintf(fp," group=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,pubdef->group_index),
                pubdef->group_index);

            fprintf(fp," segment=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,pubdef->segment_index),
                pubdef->segment_index);

            fprintf(fp," offset=0x%lX(%lu)",
                    (unsigned long)pubdef->public_offset,
                    (unsigned long)pubdef->public_offset);

            fprintf(fp," typeindex=%u",pubdef->type_index);
            fprintf(fp," %s",omf_pubdef_type_to_string(pubdef->type));
        }
        else {
            fprintf(fp," [invalid]\n");
        }
        fprintf(fp,"\n");
        i++;
    }
}

