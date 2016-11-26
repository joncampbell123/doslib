
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_parse_FIXUPP_subrecord(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    unsigned char fb;

    fb = omf_record_get_byte(rec);
    // THREAD record
    //   [7:7] 0
    //   [6:6] D [0=TARGET 1=FRAME]
    //   [5:5] 0
    //   [4:2] method
    //   [1:0] thred (sic)
    // FIXUP record
    //   [7:7] 1
    //   [6:6] M [1=segment relative 0=self-relative]
    //   [5:2] Location (4-bit field, type of location)
    //   [1:0] upper 2 bits of data record offset
    //   -----
    //   [7:0] lower 8 bits of data record offset

    if (fb & 0x80) {
        // FIXUP
        struct omf_fixupp_t *ent;

        ent = omf_fixupps_context_add_fixupp(&ctx->FIXUPPs);
        if (ent == NULL)
            return -1;

        ent->segment_relative = (fb & 0x40) ? 1 : 0;
        ent->location = (fb >> 2) & 0xF;
        ent->data_record_offset = (fb & 3) << 8;

        fb = omf_record_get_byte(rec);
        ent->data_record_offset += fb;

        fb = omf_record_get_byte(rec); // FIX Data
        // [7:7] F [1=frame datum by frame thread  0=frame field given]
        // [6:4] frame field (F=1, this is frame thread  F=0, this is frame method)
        // [3:3] T [1=target datum by target thread  0=target field given]
        // [2:2] P [1=target displacement field NOT present  1=present]
        // [1:0] Targt (T=1, this is target thread  T=0, this is target method)
        if (fb & 0x80/*F*/) {
            struct omf_fixupp_thread_t *thrd = &ctx->FIXUPPs.frame_thread[(fb >> 4) & 3];

            ent->frame_method = thrd->method;
            ent->frame_index = thrd->index;
        }
        else {
            ent->frame_method = (fb >> 4) & 7;
            if (ent->frame_method <= 2)
                ent->frame_index = omf_record_get_index(rec);
            else
                ent->frame_index = 0;
        }

        if (fb & 0x08/*T*/) {
            struct omf_fixupp_thread_t *thrd = &ctx->FIXUPPs.target_thread[fb & 3];

            ent->target_method = thrd->method;
            ent->target_index = thrd->index;
        }
        else {
            ent->target_method = fb & 3;
            ent->target_index = omf_record_get_index(rec);
        }

        if (fb & 0x04/*P*/) {
            ent->target_displacement = 0;
        }
        else {
            ent->target_displacement = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        }

        /* fixup: if the frame method is F4 (segment index of previous LEDATA record), then
         *        simplify this code by patching in the LEDATA record's segment index */
        if (ent->frame_method == 4) {
            ent->frame_method = 0;/*SEGDEF*/
            ent->frame_index = ctx->last_LEDATA_seg;
        }

        ent->omf_rec_file_enoffs = ctx->last_LEDATA_eno;
        ent->omf_rec_file_offset = ctx->last_LEDATA_rec;
        ent->omf_rec_file_header = ctx->last_LEDATA_hdr;
    }
    else {
        struct omf_fixupp_thread_t *thrd;

        // WARNING: NOT TESTED, OPEN WATCOM DOES NOT USE THIS!

        // THREAD
        if ((fb & 0xA0/*1x10 0000*/) != 0)
            return -1;

        if (fb & 0x40)/*D bit*/ {
            thrd = &ctx->FIXUPPs.frame_thread[fb&3];
            thrd->method = (fb >> 2) & 7;
        }
        else {
            thrd = &ctx->FIXUPPs.target_thread[fb&3];
            thrd->method = (fb >> 2) & 3;
        }

        thrd->alloc = 1;
        if (thrd->method <= 2)
            thrd->index = omf_record_get_index(rec);
        else
            thrd->index = 0;
    }

    return 0;
}

int omf_context_parse_FIXUPP(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_fixupps_context_get_next_add_index(&ctx->FIXUPPs);

    while (!omf_record_eof(rec)) {
        if (omf_context_parse_FIXUPP_subrecord(ctx,rec) < 0)
            return -1;
    }

    return first_entry;
}

