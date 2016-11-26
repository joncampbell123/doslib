
#include <fmt/omf/omf.h>

void dump_EXTDEF(FILE *fp,const struct omf_context_t * const ctx,unsigned int i) {
    fprintf(fp,"EXTDEF (%u):\n",i);

    while (i <= omf_extdefs_context_get_highest_index(&ctx->EXTDEFs)) {
        const struct omf_extdef_t *extdef = omf_extdefs_context_get_extdef(&ctx->EXTDEFs,i);

        fprintf(fp,"    [%u] ",i);
        if (extdef != NULL) {
            if (extdef->name_string != NULL)
                fprintf(fp,"\"%s\"",extdef->name_string);

            fprintf(fp," typeindex=%u",extdef->type_index);
            fprintf(fp," %s",omf_extdef_type_to_string(extdef->type));
        }
        else {
            fprintf(fp," [invalid]\n");
        }
        fprintf(fp,"\n");
        i++;
    }
}

