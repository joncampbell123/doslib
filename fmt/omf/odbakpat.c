
#include <fmt/omf/omf.h>

static const char *bakpat_location_to_str(const unsigned char t) {
    switch (t) {
        case 0:     return "BYTE";
        case 1:     return "WORD";
        case 2:     return "DWORD";
    };

    return "?";
}

// offset and value pairs: words, or dwords in a 32-bit record. The linker adds each value to the data at its offset.
static void dump_bakpat_pairs(FILE *fp,struct omf_record_t * const rec) {
    const unsigned int sz = (rec->rectype & 1) ? 4u : 2u;
    unsigned long ofs,val;

    while (omf_record_data_available(rec) >= (sz * 2u)) {
        ofs = (sz == 4u) ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        val = (sz == 4u) ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        fprintf(fp,"    offset=0x%lX value=0x%lX\n",ofs,val);
    }

    if (!omf_record_eof(rec))
        fprintf(fp,"    [%u bytes left over]\n",(unsigned int)omf_record_data_available(rec));
}

// BAKPAT: segment (index), location type (byte), then offset and value pairs
void dump_BAKPAT(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned char loc;
    unsigned int seg;

    if (omf_record_data_available(rec) < 2) {
        fprintf(fp,"BAKPAT [record too short]\n");
        return;
    }

    seg = omf_record_get_index(rec);
    loc = omf_record_get_byte(rec);
    fprintf(fp,"BAKPAT segment=\"%s\"(%u) location=%s(%u)\n",
        omf_context_get_segdef_name_safe(ctx,seg),seg,
        bakpat_location_to_str(loc),loc);

    dump_bakpat_pairs(fp,rec);
}

// NBKPAT: location type (byte), COMDAT name (LNAMES index), then offset and value pairs
void dump_NBKPAT(FILE *fp,const struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned char loc;
    unsigned int name;

    if (omf_record_data_available(rec) < 2) {
        fprintf(fp,"NBKPAT [record too short]\n");
        return;
    }

    loc = omf_record_get_byte(rec);
    name = omf_record_get_index(rec);
    fprintf(fp,"NBKPAT \"%s\"(%u) location=%s(%u)\n",
        omf_lnames_context_get_name_safe(&ctx->LNAMEs,name),name,
        bakpat_location_to_str(loc),loc);

    dump_bakpat_pairs(fp,rec);
}

