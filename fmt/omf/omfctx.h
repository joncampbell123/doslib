
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

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_grpdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
void omf_context_init(struct omf_context_t * const ctx);
void omf_context_free(struct omf_context_t * const ctx);
struct omf_context_t *omf_context_create(void);
struct omf_context_t *omf_context_destroy(struct omf_context_t * const ctx);
void omf_context_begin_file(struct omf_context_t * const ctx);
void omf_context_begin_module(struct omf_context_t * const ctx);
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
int omf_context_parse_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);
int omf_context_parse_LIDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);

