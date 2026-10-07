
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

// COMDEF communal length field:
//   0x00-0x80          the value itself
//   0x81 + 2 bytes     16-bit value
//   0x84 + 3 bytes     24-bit value
//   0x88 + 4 bytes     32-bit value
static int omf_record_get_comdef_length(struct omf_record_t * const rec,unsigned long * const len) {
    unsigned int bytes,i;
    unsigned char c;

    if (omf_record_eof(rec))
        return -1;

    c = omf_record_get_byte(rec);
    if (c <= 0x80) {
        *len = c;
        return 0;
    }

    if (c == 0x81)
        bytes = 2;
    else if (c == 0x84)
        bytes = 3;
    else if (c == 0x88)
        bytes = 4;
    else
        return -1;

    if (omf_record_data_available(rec) < bytes)
        return -1;

    *len = 0;
    for (i=0;i < bytes;i++)
        *len |= (unsigned long)omf_record_get_byte(rec) << (i * 8u);

    return 0;
}

// COMDEF and LCOMDEF define communal variables. Each one also takes the next EXTDEF index.
int omf_context_parse_COMDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_extdefs_context_get_next_add_index(&ctx->EXTDEFs);
    unsigned long count,size;
    int len;

    while (!omf_record_eof(rec)) {
        struct omf_extdef_t *extdef = omf_extdefs_context_add_extdef(&ctx->EXTDEFs);

        if (extdef == NULL)
            return -1;

        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_extdefs_context_set_extdef_name(&ctx->EXTDEFs,extdef,omf_temp_str,len) < 0)
            return -1;

        if (omf_record_eof(rec))
            return -1;

        extdef->type = (rec->rectype == OMF_RECTYPE_LCOMDEF) ? OMF_EXTDEF_TYPE_LOCAL : OMF_EXTDEF_TYPE_GLOBAL;
        extdef->record_type = rec->rectype;
        extdef->type_index = omf_record_get_index(rec);

        if (omf_record_eof(rec))
            return -1;

        // FAR data gives the number of elements, then the element size.
        // NEAR data (or a Borland segment index) gives the size.
        extdef->communal_data_type = omf_record_get_byte(rec);
        if (omf_record_get_comdef_length(rec,&count) < 0)
            return -1;

        if (extdef->communal_data_type == OMF_COMDEF_FAR) {
            if (omf_record_get_comdef_length(rec,&size) < 0)
                return -1;

            count *= size;
        }

        extdef->communal_length = count;
    }

    return first_entry;
}
