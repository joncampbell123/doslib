
#include <fmt/omf/omf.h>

int omf_context_generate_FIXUPP(struct omf_record_t * const rec,const struct omf_context_t * const ctx,const unsigned char is32bit) {
    const struct omf_fixupp_t *fixupp;
    unsigned int i;

    omf_record_clear(rec);

    rec->rectype = is32bit ? 0x9D : 0x9C;

    for (i=1;i <= omf_fixupps_context_get_highest_index(&ctx->FIXUPPs);i++) {
        fixupp = omf_fixupps_context_get_fixupp(&ctx->FIXUPPs,i);
        if (fixupp == NULL) continue;
        if (!fixupp->alloc) continue;

        // NTS: Write FIXUP records only, no "THREADs". keep it simple.
        omf_record_write_byte(rec,0x80/*FIXUP*/ + (fixupp->segment_relative << 6) +
            (fixupp->location << 2) + ((fixupp->data_record_offset >> 8) & 3));
        omf_record_write_byte(rec,fixupp->data_record_offset & 0xFF);

        // F=0 frame field=method T=0 P=1 Targt=method
        omf_record_write_byte(rec,(fixupp->frame_method << 4) +
            (fixupp->target_displacement != 0 ? 0x00 : 0x04) +
            (fixupp->target_method & 3));
        if (fixupp->frame_method <= 2)
            omf_record_write_index(rec,fixupp->frame_index);
        omf_record_write_index(rec,fixupp->target_index);
        if (fixupp->target_displacement != 0) {
            if (is32bit)
                omf_record_write_dword(rec,fixupp->target_displacement);
            else
                omf_record_write_word(rec,fixupp->target_displacement);
        }
    }

    omf_record_write_update_reclen(rec);
    omf_record_write_update_checksum(rec);
    return 0;
}

