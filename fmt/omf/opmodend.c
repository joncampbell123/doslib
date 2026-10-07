
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

// name of the first SEGDEF in a GRPDEF, or NULL if none.
// a frame that is a group (GRPDEF) can use any segment in that group, they all have the same base.
const char *omf_context_get_grpdef_first_segdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
    int segdef;

    if (grpdef == NULL || grpdef->count == 0)
        return NULL;

    segdef = omf_grpdefs_context_get_grpdef_segdef(&ctx->GRPDEFs,grpdef,0);
    if (segdef <= 0)
        return NULL;

    return omf_context_get_segdef_name(ctx,(unsigned int)segdef);
}

// MODEND
//   Module type (byte)             [7] main module [6] start address present [0] start address is relocatable
//   if a start address is present:
//     End data (byte)              same as FIXUPP "Fix Data": [7] F [6:4] frame [3] T [2] P [1:0] target
//     Frame datum (index)          if F=0 and the frame method is SEGDEF, GRPDEF, or EXTDEF
//     Target datum (index)         if T=0
//     Target displacement          if P=0 (word, or dword in MODEND32)
int omf_context_parse_MODEND(struct omf_context_t * const ctx,struct omf_modend_t * const modend,struct omf_record_t * const rec) {
    unsigned char end_data;

    memset(modend,0,sizeof(*modend));

    if (omf_record_eof(rec))
        return -1;

    modend->module_type = omf_record_get_byte(rec);
    modend->has_start = (modend->module_type & 0x40) ? 1 : 0;
    if (!modend->has_start)
        return 0;

    if (omf_record_eof(rec))
        return -1;

    end_data = omf_record_get_byte(rec);

    if (end_data & 0x80/*F*/) {
        const struct omf_fixupp_thread_t *thrd = &ctx->FIXUPPs.frame_thread[(end_data >> 4) & 3];

        modend->frame_method = thrd->method;
        modend->frame_index = thrd->index;
    }
    else {
        modend->frame_method = (end_data >> 4) & 7;
        if (modend->frame_method <= 2) {
            if (omf_record_eof(rec))
                return -1;

            modend->frame_index = omf_record_get_index(rec);
        }
    }

    if (end_data & 0x08/*T*/) {
        const struct omf_fixupp_thread_t *thrd = &ctx->FIXUPPs.target_thread[end_data & 3];

        modend->target_method = thrd->method;
        modend->target_index = thrd->index;
    }
    else {
        if (omf_record_eof(rec))
            return -1;

        modend->target_method = end_data & 3;
        modend->target_index = omf_record_get_index(rec);
    }

    if (!(end_data & 0x04/*P*/)) {
        if (omf_record_data_available(rec) < ((rec->rectype & 1)/*32-bit*/ ? 4u : 2u))
            return -1;

        modend->target_displacement = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);
    }

    return 0;
}
