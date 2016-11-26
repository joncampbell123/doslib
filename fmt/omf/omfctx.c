
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

