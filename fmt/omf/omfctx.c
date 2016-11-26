
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "omfrecs.h"
#include "omfcstr.h"
#include "omfrec.h"
#include "olnames.h"
#include "osegdefs.h"
#include "osegdeft.h"
#include "ogrpdefs.h"
#include "oextdefs.h"
#include "oextdeft.h"
#include "opubdefs.h"
#include "opubdeft.h"
#include "omledata.h"
#include "ofixupps.h"
#include "ofixuppt.h"
#include "omfctx.h"

char                                    omf_temp_str[255+1/*NUL*/];

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    if (i > 0) {
        const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
        if (grpdef == NULL) return NULL;
        return omf_lnames_context_get_name(&ctx->LNAMEs,grpdef->group_name_index);
    }

    return ""; // group index == 0 means segment is not part of a group
}

const char *omf_context_get_grpdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_grpdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

const char *omf_context_get_segdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,i);
    if (segdef == NULL) return NULL;
    return omf_lnames_context_get_name(&ctx->LNAMEs,segdef->segment_name_index);
}

const char *omf_context_get_segdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_segdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}

const char *omf_context_get_extdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_extdef_t *extdef = omf_extdefs_context_get_extdef(&ctx->EXTDEFs,i);
    if (extdef == NULL) return NULL;
    return extdef->name_string;
}

const char *omf_context_get_extdef_name_safe(const struct omf_context_t * const ctx,unsigned int i) {
    const char *r = omf_context_get_extdef_name(ctx,i);
    return (r != NULL) ? r : "[ERANGE]";
}
 
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

void omf_context_begin_file(struct omf_context_t * const ctx) {
    omf_fixupps_context_free_entries(&ctx->FIXUPPs);
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_begin_module(struct omf_context_t * const ctx) {
    omf_fixupps_context_free_entries(&ctx->FIXUPPs);
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_clear(struct omf_context_t * const ctx) {
    omf_fixupps_context_free_entries(&ctx->FIXUPPs);
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
    ctx->last_LEDATA_seg = 0;
    ctx->last_LEDATA_rec = 0;
    ctx->last_LEDATA_eno = 0;
    ctx->last_LEDATA_hdr = 0;
}

int omf_context_next_lib_module_fd(struct omf_context_t * const ctx,int fd) {
    unsigned long ofs;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was not a MODEND, then stop reading.
    if ((ctx->record.rectype&0xFE) != 0x8A) { // Not 0x8A or 0x8B
        errno = EIO;
        return -1;
    }

    // if we don't have a block size, then we cannot advance
    if (ctx->library_block_size == 0UL)
        return 0;

    // where does the next block size start?
    ofs = ctx->record.rec_file_offset + 3 + ctx->record.reclen;
    ofs += ctx->library_block_size - 1UL;
    ofs -= ofs % ctx->library_block_size;
    if (lseek(fd,(off_t)ofs,SEEK_SET) != (off_t)ofs)
        return 0;

    ctx->record.rec_file_offset = ofs;
    ctx->record.rectype = 0;
    ctx->record.reclen = 0;
    return 1;
}

int omf_context_read_fd(struct omf_context_t * const ctx,int fd) {
    unsigned char sum = 0;
    unsigned char tmp[3];
    unsigned int i;
    int ret;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was a MODEND, then stop reading, make caller move to next module with another function
    if ((ctx->record.rectype&0xFE) == 0x8A) // 0x8A or 0x8B
        return 0;

    ctx->last_error = NULL;
    omf_record_clear(&ctx->record);
    if (ctx->record.data == NULL && omf_record_data_alloc(&ctx->record,0) < 0)
        return -1; // sets errno
    if (ctx->record.data_alloc < 16) {
        ctx->last_error = "Record buffer too small";
        errno = EINVAL;
        return -1;
    }

    ctx->record.rec_file_offset = lseek(fd,0,SEEK_CUR);

    if ((ret=read(fd,tmp,3)) != 3) {
        if (ret >= 0) {
            return 0; // EOF
        }

        ctx->last_error = "Reading OMF record header failed";
        // read sets errno
        return -1;
    }
    ctx->record.rectype = tmp[0];
    ctx->record.reclen = *((uint16_t*)(tmp+1)); // length (including checksum)
    if (ctx->record.rectype == 0 || ctx->record.reclen == 0)
        return 0;
    if (ctx->record.reclen > ctx->record.data_alloc) {
        ctx->last_error = "Reading OMF record failed because record too large for buffer";
        errno = ERANGE;
        return -1;
    }
    if ((unsigned int)(ret=read(fd,ctx->record.data,ctx->record.reclen)) != (unsigned int)ctx->record.reclen) {
        ctx->last_error = "Reading OMF record contents failed";
        if (ret >= 0) errno = EIO;
        return -1;
    }

    /* check checksum */
    if (ctx->record.data[ctx->record.reclen-1] != 0/*optional*/) {
        for (i=0;i < 3;i++)
            sum += tmp[i];
        for (i=0;i < ctx->record.reclen;i++)
            sum += ctx->record.data[i];

        if (sum != 0) {
            ctx->last_error = "Reading OMF record checksum failed";
            errno = EIO;
            return -1;
        }
    }

    /* remember LIBHEAD block size */
    if (ctx->record.rectype == 0xF0/*LIBHEAD*/) {
        if (ctx->library_block_size == 0) {
            // and the length of the record defines the block size that modules within are aligned by
            ctx->library_block_size = ctx->record.reclen + 3;
        }
        else {
            ctx->last_error = "LIBHEAD defined again";
            errno = EIO;
            return -1;
        }
    }

    ctx->record.reclen--; // omit checksum from reclen
    return 1;
}

int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec) {
    int len;

    len = omf_record_get_lenstr(omf_temp_str,sizeof(omf_temp_str),rec);
    if (len < 0) return -1;

    if (cstr_set_n(&ctx->THEADR,omf_temp_str,len) < 0)
        return -1;

    return 0;
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

int omf_context_parse_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    if (omf_ledata_parse_header(info,rec) < 0)
        return -1;

    ctx->last_LEDATA_eno = info->enum_data_offset;
    ctx->last_LEDATA_rec = rec->rec_file_offset;
    ctx->last_LEDATA_seg = info->segment_index;
    ctx->last_LEDATA_hdr = rec->recpos;
    return 0;
}

int omf_context_parse_LIDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec) {
    if (omf_lidata_parse_header(info,rec) < 0)
        return -1;

    ctx->last_LEDATA_eno = info->enum_data_offset;
    ctx->last_LEDATA_rec = rec->rec_file_offset;
    ctx->last_LEDATA_seg = info->segment_index;
    ctx->last_LEDATA_hdr = rec->recpos;
    return 0;
}

