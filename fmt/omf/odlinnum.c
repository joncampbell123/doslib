
#include <fmt/omf/omf.h>

// line number and offset pairs: line number (word), offset (word, or dword in a 32-bit record)
static void dump_line_numbers(FILE *fp,struct omf_record_t * const rec) {
    const unsigned int ofssz = (rec->rectype & 1) ? 4u : 2u;
    unsigned int line,count = 0;
    unsigned long ofs;

    while (omf_record_data_available(rec) >= (2u + ofssz)) {
        line = omf_record_get_word(rec);
        ofs = (ofssz == 4u) ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        fprintf(fp,"%s%u:0x%lX",(count % 4u) == 0 ? "    " : "  ",line,ofs);
        if ((++count % 4u) == 0)
            fprintf(fp,"\n");
    }
    if ((count % 4u) != 0)
        fprintf(fp,"\n");

    if (!omf_record_eof(rec))
        fprintf(fp,"    [%u bytes left over]\n",(unsigned int)omf_record_data_available(rec));
}

// LINNUM: base group (index), base segment (index), a frame number (word) if both are 0, then line:offset pairs
void dump_LINNUM(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned int grp,seg;

    grp = omf_record_get_index(rec);
    seg = omf_record_get_index(rec);

    fprintf(fp,"LINNUM group=\"%s\"(%u)",omf_context_get_grpdef_name_safe(ctx,grp),grp);
    if (grp == 0 && seg == 0) {
        if (omf_record_data_available(rec) >= 2)
            fprintf(fp," frame=0x%04X",omf_record_get_word(rec));
    }
    else {
        fprintf(fp," segment=\"%s\"(%u)",omf_context_get_segdef_name_safe(ctx,seg),seg);
    }
    fprintf(fp,"\n");

    dump_line_numbers(fp,rec);
}

// LINSYM: flags (byte, [0] continuation), COMDAT name (LNAMES index), then line:offset pairs
void dump_LINSYM(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned char flags;
    unsigned int name;

    if (omf_record_data_available(rec) < 2) {
        fprintf(fp,"LINSYM [record too short]\n");
        return;
    }

    flags = omf_record_get_byte(rec);
    name = omf_record_get_index(rec);
    fprintf(fp,"LINSYM \"%s\"(%u) flags=0x%02X%s\n",
        omf_lnames_context_get_name_safe(&ctx->LNAMEs,name),name,
        flags,(flags & 0x01) ? " CONTINUATION" : "");

    dump_line_numbers(fp,rec);
}

