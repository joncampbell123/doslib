/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
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

#ifndef O_BINARY
#define O_BINARY (0)
#endif
 
//================================== OMF ================================

char                                    omf_temp_str[255+1/*NUL*/];

struct omf_context_t {
    const char*                         last_error;
    struct omf_lnames_context_t         LNAMEs;
    struct omf_segdefs_context_t        SEGDEFs;
    struct omf_grpdefs_context_t        GRPDEFs;
    struct omf_extdefs_context_t        EXTDEFs;
    struct omf_pubdefs_context_t        PUBDEFs;
    struct omf_record_t                 record; // reading, during parsing
    unsigned short                      library_block_size;// is .LIB archive if nonzero
    char*                               THEADR;
    struct {
        unsigned int                    verbose:1;
    } flags;
};

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
    if (grpdef == NULL) return NULL;
    return omf_lnames_context_get_name(&ctx->LNAMEs,grpdef->group_name_index);
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
 
void omf_context_init(struct omf_context_t * const ctx) {
    omf_pubdefs_context_init(&ctx->PUBDEFs);
    omf_extdefs_context_init(&ctx->EXTDEFs);
    omf_grpdefs_context_init(&ctx->GRPDEFs);
    omf_segdefs_context_init(&ctx->SEGDEFs);
    omf_lnames_context_init(&ctx->LNAMEs);
    omf_record_init(&ctx->record);
    ctx->last_error = NULL;
    ctx->flags.verbose = 0;
    ctx->library_block_size = 0;
    ctx->THEADR = NULL;
}

void omf_context_free(struct omf_context_t * const ctx) {
    omf_pubdefs_context_free(&ctx->PUBDEFs);
    omf_extdefs_context_free(&ctx->EXTDEFs);
    omf_grpdefs_context_free(&ctx->GRPDEFs);
    omf_segdefs_context_free(&ctx->SEGDEFs);
    omf_lnames_context_free(&ctx->LNAMEs);
    omf_record_free(&ctx->record);
    cstr_free(&ctx->THEADR);
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
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
    cstr_free(&ctx->THEADR);
}

void omf_context_begin_module(struct omf_context_t * const ctx) {
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    cstr_free(&ctx->THEADR);
}

void omf_context_clear(struct omf_context_t * const ctx) {
    omf_pubdefs_context_free_entries(&ctx->PUBDEFs);
    omf_extdefs_context_free_entries(&ctx->EXTDEFs);
    omf_grpdefs_context_free_entries(&ctx->GRPDEFs);
    omf_segdefs_context_free_entries(&ctx->SEGDEFs);
    omf_lnames_context_free_names(&ctx->LNAMEs);
    omf_record_clear(&ctx->record);
    ctx->library_block_size = 0;
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

//================================== PROGRAM ================================

static char*                            in_file = NULL;   

struct omf_context_t*                   omf_state = NULL;

static void help(void) {
    fprintf(stderr,"omfdump [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to dump\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
}

void dump_LNAMES(const struct omf_context_t * const ctx,unsigned int first_newest) {
    unsigned int i = first_newest;

    if (i == 0)
        return;

    printf("LNAMES (from %u):",i);
    while (i <= omf_lnames_context_get_highest_index(&ctx->LNAMEs)) {
        const char *p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

        if (p != NULL)
            printf(" [%u]: \"%s\"",i,p);
        else
            printf(" [%u]: (null)",i);

        i++;
    }
    printf("\n");
}

void dump_THEADR(const struct omf_context_t * const ctx) {
    printf("* THEADR: ");
    if (ctx->THEADR != NULL)
        printf("\"%s\"",ctx->THEADR);
    else
        printf("(none)\n");

    printf("\n");
}

void dump_SEGDEF(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_segdef_t *segdef = omf_segdefs_context_get_segdef(&ctx->SEGDEFs,i);

    printf("SEGDEF (%u):\n",i);
    if (segdef != NULL) {
        printf("    Alignment=%s(%u) combination=%s(%u) big=%u frame=%u offset=%u use%u\n",
            omf_segdefs_alignment_to_str(segdef->attr.f.f.alignment),
            segdef->attr.f.f.alignment,
            omf_segdefs_combination_to_str(segdef->attr.f.f.alignment),
            segdef->attr.f.f.combination,
            segdef->attr.f.f.big_segment,
            segdef->attr.f.f.use32?32U:16U,
            segdef->attr.frame_number,
            segdef->attr.offset);
        printf("    Length=%lu name=\"%s\"(%u) class=\"%s\"(%u) overlay=\"%s\"(%u)\n",
            (unsigned long)segdef->segment_length,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->segment_name_index),
            segdef->segment_name_index,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->class_name_index),
            segdef->class_name_index,
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef->overlay_name_index),
            segdef->overlay_name_index);
    }
}

void dump_GRPDEF(const struct omf_context_t * const ctx,unsigned int i) {
    const struct omf_grpdef_t *grpdef = omf_grpdefs_context_get_grpdef(&ctx->GRPDEFs,i);
    unsigned int j;

    printf("GRPDEF (%u):",i);
    if (grpdef != NULL) {
        printf(" \"%s\"(%u)\n",
            omf_lnames_context_get_name_safe(&ctx->LNAMEs,grpdef->group_name_index),
            grpdef->group_name_index);

        printf("    Contains: ");
        for (j=0;j < grpdef->count;j++) {
            int segdef = omf_grpdefs_context_get_grpdef_segdef(&ctx->GRPDEFs,grpdef,j);

            if (segdef >= 0) {
                printf("\"%s\"(%u) ",
                    omf_lnames_context_get_name_safe(&ctx->LNAMEs,segdef),
                    segdef);
            }
            else {
                printf("[invalid] ");
            }
        }
        printf("\n");
    }
    else {
        printf(" [invalid]\n");
    }
}

void dump_EXTDEF(const struct omf_context_t * const ctx,unsigned int i) {
    printf("EXTDEF (%u):\n",i);

    while (i <= omf_extdefs_context_get_highest_index(&ctx->EXTDEFs)) {
        const struct omf_extdef_t *extdef = omf_extdefs_context_get_extdef(&ctx->EXTDEFs,i);

        printf("    [%u] ",i);
        if (extdef != NULL) {
            if (extdef->name_string != NULL)
                printf("\"%s\"",extdef->name_string);

            printf(" typeindex=%u",extdef->type_index);
            printf(" %s",omf_extdef_type_to_string(extdef->type));
        }
        else {
            printf(" [invalid]\n");
        }
        printf("\n");
        i++;
    }
}

void dump_PUBDEF(const struct omf_context_t * const ctx,unsigned int i) {
    printf("PUBDEF (%u):\n",i);

    while (i <= omf_pubdefs_context_get_highest_index(&ctx->PUBDEFs)) {
        const struct omf_pubdef_t *pubdef = omf_pubdefs_context_get_pubdef(&ctx->PUBDEFs,i);

        printf("    [%u] ",i);
        if (pubdef != NULL) {
            if (pubdef->name_string != NULL)
                printf("\"%s\"",pubdef->name_string);

            printf(" group=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,pubdef->group_index),
                pubdef->group_index);

            printf(" segment=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,pubdef->segment_index),
                pubdef->segment_index);

            printf(" offset=0x%lX(%lu)",
                    (unsigned long)pubdef->public_offset,
                    (unsigned long)pubdef->public_offset);

            printf(" typeindex=%u",pubdef->type_index);
            printf(" %s",omf_pubdef_type_to_string(pubdef->type));
        }
        else {
            printf(" [invalid]\n");
        }
        printf("\n");
        i++;
    }
}

void my_dumpstate(const struct omf_context_t * const ctx) {
    unsigned int i;
    const char *p;

    printf("OBJ dump state:\n");

    if (ctx->THEADR != NULL)
        printf("* THEADR: \"%s\"\n",ctx->THEADR);

    if (ctx->LNAMEs.omf_LNAMES != NULL) {
        printf("* LNAMEs:\n");
        for (i=1;i <= ctx->LNAMEs.omf_LNAMES_count;i++) {
            p = omf_lnames_context_get_name(&ctx->LNAMEs,i);

            if (p != NULL)
                printf("   [%u]: \"%s\"\n",i,p);
            else
                printf("   [%u]: (null)\n",i);
        }
    }

    if (ctx->SEGDEFs.omf_SEGDEFS != NULL) {
        for (i=1;i <= ctx->SEGDEFs.omf_SEGDEFS_count;i++)
            dump_SEGDEF(omf_state,i);
    }

    if (ctx->GRPDEFs.omf_GRPDEFS != NULL) {
        for (i=1;i <= ctx->GRPDEFs.omf_GRPDEFS_count;i++)
            dump_GRPDEF(omf_state,i);
    }

    if (ctx->EXTDEFs.omf_EXTDEFS != NULL)
        dump_EXTDEF(omf_state,1);

    if (ctx->PUBDEFs.omf_PUBDEFS != NULL)
        dump_PUBDEF(omf_state,1);

    printf("----END-----\n");
}

int main(int argc,char **argv) {
    unsigned char dumpstate = 0;
    unsigned char diddump = 0;
    unsigned char verbose = 0;
    int i,fd,ret;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                in_file = argv[i++];
                if (in_file == NULL) return 1;
            }
            else if (!strcmp(a,"v")) {
                verbose = 1;
            }
            else if (!strcmp(a,"d")) {
                dumpstate = 1;
            }
            else {
                help();
                return 1;
            }
        }
        else {
            fprintf(stderr,"Unexpected arg %s\n",a);
            return 1;
        }
    }

    // prepare parsing
    if ((omf_state=omf_context_create()) == NULL) {
        fprintf(stderr,"Failed to init OMF parsing state\n");
        return 1;
    }
    omf_state->flags.verbose = (verbose > 0);

    if (in_file == NULL) {
        help();
        return 1;
    }

    fd = open(in_file,O_RDONLY|O_BINARY);
    if (fd < 0) {
        fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
        return 1;
    }

    omf_context_begin_file(omf_state);

    do {
        ret = omf_context_read_fd(omf_state,fd);
        if (ret == 0) {
            if (omf_record_is_modend(&omf_state->record)) {
                if (dumpstate && !diddump) {
                    my_dumpstate(omf_state);
                    diddump = 1;
                }

                ret = omf_context_next_lib_module_fd(omf_state,fd);
                if (ret < 0) {
                    printf("Unable to advance to next .LIB module, %s\n",strerror(errno));
                    if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
                }
                else if (ret > 0) {
                    omf_context_begin_module(omf_state);
                    diddump = 0;
                    continue;
                }
            }

            break;
        }
        else if (ret < 0) {
            fprintf(stderr,"Error: %s\n",strerror(errno));
            if (omf_state->last_error != NULL) fprintf(stderr,"Details: %s\n",omf_state->last_error);
            break;
        }

        printf("OMF record type=0x%02x (%s: %s) length=%u offset=%lu blocksize=%u\n",
                omf_state->record.rectype,
                omf_rectype_to_str(omf_state->record.rectype),
                omf_rectype_to_str_long(omf_state->record.rectype),
                omf_state->record.reclen,
                omf_state->record.rec_file_offset,
                omf_state->library_block_size);

        switch (omf_state->record.rectype) {
            case OMF_RECTYPE_THEADR:/*0x80*/
                if (omf_context_parse_THEADR(omf_state,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing THEADR\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_THEADR(omf_state);

                break;
            case OMF_RECTYPE_EXTDEF:/*0x8C*/
            case OMF_RECTYPE_LEXTDEF:/*0xB4*/
            case OMF_RECTYPE_LEXTDEF32:/*0xB5*/{
                int first_new_extdef;

                if ((first_new_extdef=omf_context_parse_EXTDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing EXTDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_EXTDEF(omf_state,(unsigned int)first_new_extdef);

                } break;
            case OMF_RECTYPE_PUBDEF:/*0x90*/
            case OMF_RECTYPE_PUBDEF32:/*0x91*/
            case OMF_RECTYPE_LPUBDEF:/*0xB6*/
            case OMF_RECTYPE_LPUBDEF32:/*0xB7*/{
                int first_new_pubdef;

                if ((first_new_pubdef=omf_context_parse_PUBDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing PUBDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_PUBDEF(omf_state,(unsigned int)first_new_pubdef);

                } break;
            case OMF_RECTYPE_LNAMES:/*0x96*/{
                int first_new_lname;

                if ((first_new_lname=omf_context_parse_LNAMES(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing LNAMES\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_LNAMES(omf_state,(unsigned int)first_new_lname);

                } break;
            case OMF_RECTYPE_SEGDEF:/*0x98*/
            case OMF_RECTYPE_SEGDEF32:/*0x99*/{
                int first_new_segdef;

                if ((first_new_segdef=omf_context_parse_SEGDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing SEGDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_SEGDEF(omf_state,(unsigned int)first_new_segdef);

                } break;
            case OMF_RECTYPE_GRPDEF:/*0x9A*/
            case OMF_RECTYPE_GRPDEF32:/*0x9B*/{
                int first_new_grpdef;

                if ((first_new_grpdef=omf_context_parse_GRPDEF(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing GRPDEF\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_GRPDEF(omf_state,(unsigned int)first_new_grpdef);

                } break;

        }
#if 0
            switch (omf_rectype) {
                case 0x82:/* LHEADR */
                    dump_LHEADR();
                    break;
                case 0x88:/* COMENT */
                    dump_COMENT();
                    break;
                case 0x8A:/* MODEND */
                case 0x8B:/* MODEND32 */
                    dump_MODEND(omf_rectype&1);
                    break;
                case 0x90:/* PUBDEF */
                case 0x91:/* PUBDEF32 */
                    dump_PUBDEF(omf_rectype&1);
                    break;
                case 0x9C:/* FIXUPP */
                case 0x9D:/* FIXUPP32 */
                    dump_FIXUPP(omf_rectype&1);
                    break;
                case 0xA0:/* LEDATA */
                case 0xA1:/* LEDATA32 */
                    dump_LEDATA(omf_rectype&1);
                    break;
                case 0xA2:/* LIDATA */
                case 0xA3:/* LIDATA32 */
                    dump_LIDATA(omf_rectype&1);
                    break;
                case 0xB4:/* LEXTDEF */
                case 0xB5:/* LEXTDEF32 */
                    dump_LEXTDEF(omf_rectype&1);
                    break;
                case 0xB6:/* LPUBDEF */
                case 0xB7:/* LPUBDEF32 */
                    dump_LPUBDEF(omf_rectype&1);
                    break;
                case 0xF0:/* LIBHEAD */
                    dump_LIBHEAD();
                    break;
                case 0xF1:/* LIBEND */
                    dump_LIBEND();
                    break;
                default:
                    printf("** do not yet support\n");
                    break;
            }

            omfrec_checkrange();
            if (omf_rectype == 0xF1) break; // stop at LIBEND. there's non-OMF records that usually follow it.
#endif
    } while (1);

    if (dumpstate && !diddump) {
        my_dumpstate(omf_state);
        diddump = 1;
    }

    omf_context_clear(omf_state);
    omf_state = omf_context_destroy(omf_state);
    close(fd);
    return 0;
}

