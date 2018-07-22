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

#define MAX_IN_FILES                    256

static char*                            out_file = NULL;

static char*                            in_file[MAX_IN_FILES];
static unsigned int                     in_file_count = 0;

struct omf_context_t*                   omf_state = NULL;

static void help(void) {
    fprintf(stderr,"lnkdos16 [options]\n");
    fprintf(stderr,"  -i <file>    OMF file to link\n");
    fprintf(stderr,"  -o <file>    Output file\n");
    fprintf(stderr,"  -v           Verbose mode\n");
    fprintf(stderr,"  -d           Dump memory state after parsing\n");
}

void dump_COMENT(FILE *fp,struct omf_context_t * const ctx) {
    unsigned char comment_type;
    unsigned char comment_class;

    fprintf(fp,"COMENT:\n");

    comment_type = omf_record_get_byte(&ctx->record);
    comment_class = omf_record_get_byte(&ctx->record);
    fprintf(fp,"    Comment Type:     0x%02x ",comment_type);
    if (comment_type & 0x80) fprintf(fp,"NO-PURGE ");
    if (comment_type & 0x40) fprintf(fp,"NO-LIST ");
    fprintf(fp,"\n");

    fprintf(fp,"    Comment Class:    0x%02x ",comment_class);
    if (comment_class == 0xA0) { /* microsoft extension */
        unsigned char subtype = omf_record_get_byte(&ctx->record);

        switch (subtype) {
            case 0x01:  fprintf(fp,"IMPDEF"); break;
            case 0x02:  fprintf(fp,"EXPDEF"); break;
            case 0x03:  fprintf(fp,"INCDEF"); break;
            case 0x04:  fprintf(fp,"Protected Memory Library"); break;
            case 0x05:  fprintf(fp,"LNKDIR"); break;
            case 0x06:  fprintf(fp,"Big-endian"); break;
            case 0x07:  fprintf(fp,"PRECOMP"); break;
        };
    }
    else if (comment_class == 0xA1) {
        fprintf(fp,"New OMF extension");
    }
    else if (comment_class == 0xE9) {
        fprintf(fp,"Dependency file");
    }
    fprintf(fp,"\n");
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
            dump_SEGDEF(stdout,omf_state,i);
    }

    if (ctx->GRPDEFs.omf_GRPDEFS != NULL) {
        for (i=1;i <= ctx->GRPDEFs.omf_GRPDEFS_count;i++)
            dump_GRPDEF(stdout,omf_state,i);
    }

    if (ctx->EXTDEFs.omf_EXTDEFS != NULL)
        dump_EXTDEF(stdout,omf_state,1);

    if (ctx->PUBDEFs.omf_PUBDEFS != NULL)
        dump_PUBDEF(stdout,omf_state,1);

    if (ctx->FIXUPPs.omf_FIXUPPS != NULL)
        dump_FIXUPP(stdout,omf_state,1);

    printf("----END-----\n");
}

int main(int argc,char **argv) {
    unsigned char verbose = 0;
    unsigned char diddump = 0;
    unsigned int inf;
    int i,fd,ret;
    char *a;

    for (i=1;i < argc;) {
        a = argv[i++];

        if (*a == '-') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"i")) {
                if (in_file_count >= MAX_IN_FILES) {
                    fprintf(stderr,"Too many input files\n");
                    return 1;
                }

                in_file[in_file_count] = argv[i++];
                if (in_file[in_file_count] == NULL) return 1;
                in_file_count++;
            }
            else if (!strcmp(a,"o")) {
                out_file = argv[i++];
                if (out_file == NULL) return 1;
            }
            else if (!strcmp(a,"v")) {
                verbose = 1;
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

    if (in_file_count == 0) {
        help();
        return 1;
    }

    if (out_file == NULL) {
        help();
        return 1;
    }

    for (inf=0;inf < in_file_count;inf++) {
        assert(in_file[inf] != NULL);

        fd = open(in_file[inf],O_RDONLY|O_BINARY);
        if (fd < 0) {
            fprintf(stderr,"Failed to open input file %s\n",strerror(errno));
            return 1;
        }

        // prepare parsing
        if ((omf_state=omf_context_create()) == NULL) {
            fprintf(stderr,"Failed to init OMF parsing state\n");
            return 1;
        }
        omf_state->flags.verbose = (verbose > 0);

        diddump = 0;
        omf_context_begin_file(omf_state);

        do {
            ret = omf_context_read_fd(omf_state,fd);
            if (ret == 0) {
                if (omf_record_is_modend(&omf_state->record)) {
                    if (!diddump) {
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
                        dump_THEADR(stdout,omf_state);

                    break;
                case OMF_RECTYPE_COMENT:/*0x88*/
                    if (omf_state->flags.verbose)
                        dump_COMENT(stdout,omf_state);
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
                                                           dump_EXTDEF(stdout,omf_state,(unsigned int)first_new_extdef);

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
                                                           dump_PUBDEF(stdout,omf_state,(unsigned int)first_new_pubdef);

                                                   } break;
                case OMF_RECTYPE_LNAMES:/*0x96*/{
                                                    int first_new_lname;

                                                    if ((first_new_lname=omf_context_parse_LNAMES(omf_state,&omf_state->record)) < 0) {
                                                        fprintf(stderr,"Error parsing LNAMES\n");
                                                        return 1;
                                                    }

                                                    if (omf_state->flags.verbose)
                                                        dump_LNAMES(stdout,omf_state,(unsigned int)first_new_lname);

                                                } break;
                case OMF_RECTYPE_SEGDEF:/*0x98*/
                case OMF_RECTYPE_SEGDEF32:/*0x99*/{
                                                      int first_new_segdef;

                                                      if ((first_new_segdef=omf_context_parse_SEGDEF(omf_state,&omf_state->record)) < 0) {
                                                          fprintf(stderr,"Error parsing SEGDEF\n");
                                                          return 1;
                                                      }

                                                      if (omf_state->flags.verbose)
                                                          dump_SEGDEF(stdout,omf_state,(unsigned int)first_new_segdef);

                                                  } break;
                case OMF_RECTYPE_GRPDEF:/*0x9A*/
                case OMF_RECTYPE_GRPDEF32:/*0x9B*/{
                                                      int first_new_grpdef;

                                                      if ((first_new_grpdef=omf_context_parse_GRPDEF(omf_state,&omf_state->record)) < 0) {
                                                          fprintf(stderr,"Error parsing GRPDEF\n");
                                                          return 1;
                                                      }

                                                      if (omf_state->flags.verbose)
                                                          dump_GRPDEF(stdout,omf_state,(unsigned int)first_new_grpdef);

                                                  } break;
                case OMF_RECTYPE_FIXUPP:/*0x9C*/
                case OMF_RECTYPE_FIXUPP32:/*0x9D*/{
                                                      int first_new_fixupp;

                                                      if ((first_new_fixupp=omf_context_parse_FIXUPP(omf_state,&omf_state->record)) < 0) {
                                                          fprintf(stderr,"Error parsing FIXUPP\n");
                                                          return 1;
                                                      }

                                                      if (omf_state->flags.verbose)
                                                          dump_FIXUPP(stdout,omf_state,(unsigned int)first_new_fixupp);

                                                  } break;
                case OMF_RECTYPE_LEDATA:/*0xA0*/
                case OMF_RECTYPE_LEDATA32:/*0xA1*/{
                                                      struct omf_ledata_info_t info;

                                                      if (omf_context_parse_LEDATA(omf_state,&info,&omf_state->record) < 0) {
                                                          fprintf(stderr,"Error parsing LEDATA\n");
                                                          return 1;
                                                      }

                                                      if (omf_state->flags.verbose)
                                                          dump_LEDATA(stdout,omf_state,&info);

                                                  } break;
                case OMF_RECTYPE_LIDATA:/*0xA2*/
                case OMF_RECTYPE_LIDATA32:/*0xA3*/{
                                                      struct omf_ledata_info_t info;

                                                      if (omf_context_parse_LIDATA(omf_state,&info,&omf_state->record) < 0) {
                                                          fprintf(stderr,"Error parsing LIDATA\n");
                                                          return 1;
                                                      }

                                                      if (omf_state->flags.verbose)
                                                          dump_LIDATA(stdout,omf_state,&info,&omf_state->record);

                                                  } break;
            }
        } while (1);

        if (!diddump) {
            my_dumpstate(omf_state);
            diddump = 1;
        }

        omf_context_clear(omf_state);
        omf_state = omf_context_destroy(omf_state);

        close(fd);
    }

    return 0;
}

