/* FIXME: This code (and omfsegfl) should be consolidated into a library for
 *        reading/writing OMF files. */

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

#include <fmt/omf/omf.h>

#ifndef O_BINARY
#define O_BINARY (0)
#endif

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

static void print_b(unsigned char c) {
    if (c >= 32 && c < 127)
        printf("%c",(char)c);
    else
        printf(".");
}

void dump_LIDATA(const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info,const struct omf_record_t * const rec) {
    (void)rec;

    printf("LIDATA segment=\"%s\"(%u) data_offset=0x%lX(%lu) clength=0x%lX(%lu)\n",
        omf_context_get_segdef_name_safe(ctx,info->segment_index),
        info->segment_index,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);
}

void dump_LEDATA(const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info) {
    unsigned int col,colstart;
    unsigned long i,pos,ics;
    unsigned int ci;

    printf("LEDATA segment=\"%s\"(%u) data_offset=0x%lX(%lu) length=0x%lX(%lu)\n",
        omf_context_get_segdef_name_safe(ctx,info->segment_index),
        info->segment_index,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);

    i = 0;
    col = 0;
    pos = info->enum_data_offset;
    colstart = (unsigned int)(pos & 0xFUL);
    while (i < info->data_length) {
        if (col == 0) {
            printf("    0x%08lX: ",pos & (~0xFUL));
            ics = i;
        }

        while (col < (unsigned int)(pos & 0xFUL)) {
            printf("  %c",col==7?'-':' ');
            col++;
        }

        printf("%02X%c",info->data[i],col==7?'-':' ');
        col++;
        pos++;
        i++;

        if (col == 0x10) {
            ci=0;
            while (ci < colstart) {
                printf(" ");
                ci++;
            }

            assert((ics+ci) >= colstart);
            while (ci < 16) {
                print_b(info->data[ics+ci-colstart]);
                ci++;
            }

            colstart = 0;
            printf("\n");
            col = 0;
        }
    }

    if (col != 0) {
        unsigned int colend = col;

        while (col < 16) {
            printf("  %c",col==7?'-':' ');
            col++;
        }

        ci=0;
        while (ci < colstart) {
            printf(" ");
            ci++;
        }

        assert((ics+ci) >= colstart);
        while (ci < colend) {
            print_b(info->data[ics+ci-colstart]);
            ci++;
        }

        printf("\n");
    }
}

void dump_FIXUPP_entry(const struct omf_context_t * const ctx,const struct omf_fixupp_t * const ent) {
    printf("    %s-relative location=%s(%u) frame_method=%s(%u)",
            ent->segment_relative?"seg":"self",
            omf_fixupp_location_to_str(ent->location),
            ent->location,
            omf_fixupp_frame_method_to_str(ent->frame_method),
            ent->frame_method);

    if (ent->frame_method == 0/*SEGDEF*/) {
        printf(" frame_index=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }
    else if (ent->frame_method == 1/*GRPDEF*/) {
        printf(" frame_index=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }
    else if (ent->frame_method == 2/*EXTDEF*/) {
        printf(" frame_index=\"%s\"(%u)",
                omf_context_get_extdef_name_safe(ctx,ent->frame_index),
                ent->frame_index);
    }

    printf("\n");

    printf("    target_method=\"%s\"(%u)",
            omf_fixupp_target_method_to_str(ent->target_method),
            ent->target_method);

    if (ent->target_method == 0/*SEGDEF*/) {
        printf(" target_index=\"%s\"(%u)",
                omf_context_get_segdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }
    else if (ent->target_method == 1/*GRPDEF*/) {
        printf(" target_index=\"%s\"(%u)",
                omf_context_get_grpdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }
    else if (ent->target_method == 2/*EXTDEF*/) {
        printf(" target_index=\"%s\"(%u)",
                omf_context_get_extdef_name_safe(ctx,ent->target_index),
                ent->target_index);
    }

    printf(" data_rec_ofs=0x%lX(%lu)\n",
            (unsigned long)ent->data_record_offset,
            (unsigned long)ent->data_record_offset);
    printf("    target_displacement=%lu ledata_rec_ofs=0x%lX(%lu)+%u absrecofs=0x%lX(%lu)\n",
            (unsigned long)ent->target_displacement,
            (unsigned long)ent->omf_rec_file_offset,
            (unsigned long)ent->omf_rec_file_offset,
            (unsigned int)ent->omf_rec_file_header,
            (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset,
            (unsigned long)ent->omf_rec_file_enoffs + (unsigned long)ent->data_record_offset);
}

void dump_FIXUPP(const struct omf_context_t * const ctx,unsigned int i) {
    while (i <= omf_fixupps_context_get_highest_index(&ctx->FIXUPPs)) {
        const struct omf_fixupp_t *ent = omf_fixupps_context_get_fixupp(&ctx->FIXUPPs,i);

        printf("FIXUPP[%u]:\n",i);
        if (ent != NULL) dump_FIXUPP_entry(ctx,ent);

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

    if (ctx->FIXUPPs.omf_FIXUPPS != NULL)
        dump_FIXUPP(omf_state,1);

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

                printf("----- next module -----\n");

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
            case OMF_RECTYPE_FIXUPP:/*0x9C*/
            case OMF_RECTYPE_FIXUPP32:/*0x9D*/{
                int first_new_fixupp;

                if ((first_new_fixupp=omf_context_parse_FIXUPP(omf_state,&omf_state->record)) < 0) {
                    fprintf(stderr,"Error parsing FIXUPP\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_FIXUPP(omf_state,(unsigned int)first_new_fixupp);

                } break;
            case OMF_RECTYPE_LEDATA:/*0xA0*/
            case OMF_RECTYPE_LEDATA32:/*0xA1*/{
                struct omf_ledata_info_t info;

                if (omf_context_parse_LEDATA(omf_state,&info,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing LEDATA\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_LEDATA(omf_state,&info);

                } break;
            case OMF_RECTYPE_LIDATA:/*0xA2*/
            case OMF_RECTYPE_LIDATA32:/*0xA3*/{
                struct omf_ledata_info_t info;

                if (omf_context_parse_LIDATA(omf_state,&info,&omf_state->record) < 0) {
                    fprintf(stderr,"Error parsing LIDATA\n");
                    return 1;
                }

                if (omf_state->flags.verbose)
                    dump_LIDATA(omf_state,&info,&omf_state->record);

                } break;
        }
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

