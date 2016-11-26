
#ifndef _DOSLIB_OMF_OPLEDATA_H
#define _DOSLIB_OMF_OPLEDATA_H

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
#include <fmt/omf/omfctx.h>

int omf_context_parse_LEDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);
int omf_context_parse_LIDATA(struct omf_context_t * const ctx,struct omf_ledata_info_t * const info,struct omf_record_t * const rec);

#endif //_DOSLIB_OMF_OPLEDATA_H

