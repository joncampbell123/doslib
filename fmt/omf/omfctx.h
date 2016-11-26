
#ifndef _DOSLIB_OMF_OMFCTX_H
#define _DOSLIB_OMF_OMFCTX_H

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

#include <fmt/omf/omfrecs.h>
#include <fmt/omf/omfcstr.h>
#include <fmt/omf/omfrec.h>
#include <fmt/omf/olnames.h>
#include <fmt/omf/osegdefs.h>
#include <fmt/omf/osegdeft.h>
#include <fmt/omf/ogrpdefs.h>
#include <fmt/omf/oextdefs.h>
#include <fmt/omf/oextdeft.h>
#include <fmt/omf/opubdefs.h>
#include <fmt/omf/opubdeft.h>
#include <fmt/omf/omledata.h>
#include <fmt/omf/ofixupps.h>
#include <fmt/omf/ofixuppt.h>

extern char                             omf_temp_str[255+1/*NUL*/];

struct omf_context_t {
    const char*                         last_error;
    struct omf_lnames_context_t         LNAMEs;
    struct omf_segdefs_context_t        SEGDEFs;
    struct omf_grpdefs_context_t        GRPDEFs;
    struct omf_extdefs_context_t        EXTDEFs;
    struct omf_pubdefs_context_t        PUBDEFs;
    struct omf_fixupps_context_t        FIXUPPs;
    struct omf_record_t                 record; // reading, during parsing
    unsigned short                      library_block_size;// is .LIB archive if nonzero
    unsigned short                      last_LEDATA_seg;
    unsigned long                       last_LEDATA_rec;
    unsigned long                       last_LEDATA_eno;
    unsigned char                       last_LEDATA_hdr;
    char*                               THEADR;
    struct {
        unsigned int                    verbose:1;
    } flags;
};

void omf_context_init(struct omf_context_t * const ctx);
void omf_context_free(struct omf_context_t * const ctx);
struct omf_context_t *omf_context_create(void);
struct omf_context_t *omf_context_destroy(struct omf_context_t * const ctx);
void omf_context_begin_file(struct omf_context_t * const ctx);
void omf_context_begin_module(struct omf_context_t * const ctx);
void omf_context_clear_for_module(struct omf_context_t * const ctx);
void omf_context_clear(struct omf_context_t * const ctx);
int omf_context_next_lib_module_fd(struct omf_context_t * const ctx,int fd);
int omf_context_read_fd(struct omf_context_t * const ctx,int fd);
int omf_context_parse_THEADR(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_LNAMES(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_SEGDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_GRPDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_EXTDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_PUBDEF(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_FIXUPP_subrecord(struct omf_context_t * const ctx,struct omf_record_t * const rec);
int omf_context_parse_FIXUPP(struct omf_context_t * const ctx,struct omf_record_t * const rec);

#endif //_DOSLIB_OMF_OMFCTX_H

