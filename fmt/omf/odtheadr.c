
#include <fmt/omf/omf.h>

void dump_THEADR(FILE *fp,const struct omf_context_t * const ctx) {
    fprintf(fp,"* THEADR: ");
    if (ctx->THEADR != NULL)
        fprintf(fp,"\"%s\"",ctx->THEADR);
    else
        fprintf(fp,"(none)\n");

    fprintf(fp,"\n");
}

