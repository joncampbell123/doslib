
#include <stdio.h>

#include <fmt/omf/omfctx.h>

const char *omf_context_get_grpdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_grpdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_segdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name(const struct omf_context_t * const ctx,unsigned int i);
const char *omf_context_get_extdef_name_safe(const struct omf_context_t * const ctx,unsigned int i);

