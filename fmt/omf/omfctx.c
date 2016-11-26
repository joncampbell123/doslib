
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

char                                    omf_temp_str[255+1/*NUL*/];
 
void omf_context_init(struct omf_context_t * const ctx) {
    omf_fixupps_context_init(&ctx->FIXUPPs);
    omf_pubdefs_context_init(&ctx->PUBDEFs);
    omf_extdefs_context_init(&ctx->EXTDEFs);
    omf_grpdefs_context_init(&ctx->GRPDEFs);
    omf_segdefs_context_init(&ctx->SEGDEFs);
    omf_lnames_context_init(&ctx->LNAMEs);
    omf_record_init(&ctx->record);
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
    ctx->last_error = NULL;
    ctx->flags.verbose = 0;
    ctx->library_block_size = 0;
    ctx->THEADR = NULL;
}

void omf_context_free(struct omf_context_t * const ctx) {
    omf_fixupps_context_free(&ctx->FIXUPPs);
    omf_pubdefs_context_free(&ctx->PUBDEFs);
    omf_extdefs_context_free(&ctx->EXTDEFs);
    omf_grpdefs_context_free(&ctx->GRPDEFs);
    omf_segdefs_context_free(&ctx->SEGDEFs);
    omf_lnames_context_free(&ctx->LNAMEs);
    omf_record_free(&ctx->record);
    cstr_free(&ctx->THEADR);
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
}

struct omf_context_t *omf_context_create(void) {
    struct omf_context_t *ctx;

    ctx = malloc(sizeof(*ctx));
    if (ctx != NULL) omf_context_init(ctx);
    return ctx;
}

struct omf_context_t *omf_context_destroy(struct omf_context_t * const ctx) {
    if (ctx != NULL) {
        omf_context_free(ctx);
        free(ctx);
    }

    return NULL;
}

void omf_context_clear_for_module(struct omf_context_t * const ctx) {
    omf_fixupps_context_free_entries(&ctx->FIXUPPs);
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_clear(struct omf_context_t * const ctx) {
    omf_context_clear_for_module(ctx);
    ctx->library_block_size = 0;
}

void omf_context_begin_file(struct omf_context_t * const ctx) {
    omf_context_clear(ctx);
}

void omf_context_begin_module(struct omf_context_t * const ctx) {
    omf_context_clear_for_module(ctx);
}

int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_lnames_context_get_next_add_index(&ctx->LNAMEs);
    int len;

    while (!omf_record_eof(rec)) {
        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_lnames_context_add_name(&ctx->LNAMEs,omf_temp_str,len) < 0)
            return -1;
    }

    return first_entry;
}

int omf_context_parse_SEGDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_segdefs_context_get_next_add_index(&ctx->SEGDEFs);
    struct omf_segdef_t *segdef = omf_segdefs_context_add_segdef(&ctx->SEGDEFs);

    if (segdef == NULL)
        return -1;

    // read segment attributes
    //   alignment      [7:5]
    //   combination    [4:2]
    //   big (max)      [1:1]
    //   use32          [0:0]
    segdef->attr.f.raw = omf_record_get_byte(rec);
    // frame number (conditional)
    if (segdef->attr.f.f.alignment == 0) {
        segdef->attr.frame_number = omf_record_get_word(rec);
        segdef->attr.offset = omf_record_get_byte(rec);
    }

    if (omf_record_eof(rec))
        return -1;

    if (rec->rectype & 1/*32-bit version*/)
        segdef->segment_length = omf_record_get_dword(rec);
    else
        segdef->segment_length = omf_record_get_word(rec);

    segdef->segment_name_index = omf_record_get_index(rec);
    segdef->class_name_index = omf_record_get_index(rec);

    if (omf_record_eof(rec))
        return -1;

    segdef->overlay_name_index = omf_record_get_index(rec);
    return first_entry;
}

int omf_context_parse_GRPDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_grpdefs_context_get_next_add_index(&ctx->GRPDEFs);
    struct omf_grpdef_t *grpdef = omf_grpdefs_context_add_grpdef(&ctx->GRPDEFs);

    if (grpdef == NULL)
        return -1;

    if (omf_record_eof(rec))
        return -1;

    grpdef->group_name_index = omf_record_get_index(rec);

    while (!omf_record_eof(rec)) {
        unsigned char index = omf_record_get_byte(rec);
        unsigned int segdef = omf_record_get_index(rec);

        // we only support index 0xFF (segdef)
        if (index != 0xFF)
            return -1;

        if (omf_grpdefs_context_add_grpdef_segdef(&ctx->GRPDEFs,grpdef,segdef) < 0)
            return -1;
    }

    return first_entry;
}

int omf_context_parse_EXTDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_extdefs_context_get_next_add_index(&ctx->EXTDEFs);
    unsigned int type;
    int len;

    if ((rec->rectype & 0xFE) == OMF_RECTYPE_LEXTDEF/*0xB4*/)
        type = OMF_EXTDEF_TYPE_LOCAL;
    else
        type = OMF_EXTDEF_TYPE_GLOBAL;

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

        extdef->type = type;
        extdef->type_index = omf_record_get_index(rec);
    }

    return first_entry;
}

int omf_context_parse_PUBDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int first_entry = omf_pubdefs_context_get_next_add_index(&ctx->PUBDEFs);
    unsigned int base_segment_index;
    unsigned int base_group_index;
    unsigned int type;
    int len;

    if ((rec->rectype & 0xFE) == OMF_RECTYPE_LPUBDEF/*0xB6*/)
        type = OMF_PUBDEF_TYPE_LOCAL;
    else
        type = OMF_PUBDEF_TYPE_GLOBAL;

    if (omf_record_eof(rec))
        return -1;

    base_group_index = omf_record_get_index(rec);
    base_segment_index = omf_record_get_index(rec);
    if (base_segment_index == 0) {
        (void)omf_record_get_word(rec); // base frame (ignored)
    }

    while (!omf_record_eof(rec)) {
        struct omf_pubdef_t *pubdef = omf_pubdefs_context_add_pubdef(&ctx->PUBDEFs);

        if (pubdef == NULL)
            return -1;

        len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
        if (len < 0) return -1;

        if (omf_pubdefs_context_set_pubdef_name(&ctx->PUBDEFs,pubdef,omf_temp_str,len) < 0)
            return -1;

        if (omf_record_eof(rec))
            return -1;

        pubdef->group_index = base_group_index;
        pubdef->segment_index = base_segment_index;
        pubdef->public_offset = (rec->rectype & 1)/*32-bit*/ ? omf_record_get_dword(rec) : omf_record_get_word(rec);
        pubdef->type_index = omf_record_get_index(rec);
        pubdef->type = type;
    }

    return first_entry;
}

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

