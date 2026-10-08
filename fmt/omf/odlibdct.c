
#include <fmt/omf/omf.h>

// list the symbols in the .LIB dictionary, and the module that defines each. returns 0, or -1 if it cannot be read.
int dump_LIBDICT(FILE *fp,const struct omf_context_t * const ctx,const int fd) {
    static unsigned char blk[OMF_LIBDICT_BLOCK_SIZE];
    unsigned int i,b,page;
    int r;

    fprintf(fp,"LIBDICT offset=0x%lX blocks=%u\n",ctx->library_dict_offset,ctx->library_dict_blocks);

    for (i=0;i < ctx->library_dict_blocks;i++) {
        if (omf_lib_dict_read_block(blk,ctx,fd,i) < 0) {
            fprintf(fp,"    [cannot read block %u]\n",i);
            return -1;
        }

        for (b=0;b < OMF_LIBDICT_BUCKETS;b++) {
            r = omf_lib_dict_get_entry(blk,b,omf_temp_str,&page);
            if (r < 0)
                fprintf(fp,"    [%u.%u] [invalid entry]\n",i,b);
            else if (r > 0)
                fprintf(fp,"    [%u.%u] \"%s\" page=%u offset=0x%lX\n",i,b,omf_temp_str,page,
                    (unsigned long)page * (unsigned long)ctx->library_block_size);
        }
    }

    return 0;
}

