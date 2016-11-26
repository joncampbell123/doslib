
#include <fmt/omf/omf.h>

void dump_LNAMES(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    if (i == 0)
        return;

    fprintf(fp,"LNAMES (from %u):",i);
    while (i <= omf_lnames_context_get_highest_index(&ctx->LNAMEs)) {
        const char *p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

        if (p != NULL)
            fprintf(fp," [%u]: \"%s\"",i,p);
        else
            fprintf(fp," [%u]: (null)",i);

        i++;
    }
    fprintf(fp,"\n");
}

