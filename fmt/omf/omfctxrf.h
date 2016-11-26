
#ifndef _DOSLIB_OMF_OMFCTXRF_H
#define _DOSLIB_OMF_OMFCTXRF_H

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

int omf_context_read_fd(struct omf_context_t * const ctx,int fd);

#endif //_DOSLIB_OMF_OMFCTXRF_H

