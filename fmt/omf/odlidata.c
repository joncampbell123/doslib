
#include <limits.h>

#include <fmt/omf/omf.h>

// how much of the expanded data dump_LIDATA_blocks() shows
#define DUMP_LIDATA_MAX_EXPANDED        (0x1000UL)

// print the data block at data offset ofs, and the blocks nested in it.
// returns the data offset after the block, or -1 if it is malformed.
static long dump_LIDATA_block(FILE *fp,const struct omf_ledata_info_t * const info,unsigned long ofs,const unsigned int depth) {
    const int indent = 4 + (int)(depth * 4u);
    struct omf_lidata_block_t blk;
    unsigned int i;
    long r;

    if (depth >= OMF_LIDATA_MAX_DEPTH || omf_lidata_get_block(&blk,info,ofs) < 0) {
        fprintf(fp,"%*s[invalid data block at 0x%lX]\n",indent,"",ofs);
        return -1;
    }

    fprintf(fp,"%*srepeat=%lu",indent,"",blk.repeat_count);
    if (blk.block_count == 0) {
        fprintf(fp," bytes=%u:",blk.content_length);
        for (i=0;i < blk.content_length;i++)
            fprintf(fp," %02X",info->data[blk.content_offset+i]);
        fprintf(fp,"\n");

        return (long)(blk.content_offset + blk.content_length);
    }

    fprintf(fp," blocks=%u\n",blk.block_count);
    ofs = blk.content_offset;
    for (i=0;i < blk.block_count;i++) {
        if ((r=dump_LIDATA_block(fp,info,ofs,depth+1)) < 0)
            return -1;

        ofs = (unsigned long)r;
    }

    return (long)ofs;
}

// print the iterated data blocks of an LIDATA record (or of a COMDAT with the iterated flag),
// then a hex dump of the data they expand to.
void dump_LIDATA_blocks(FILE *fp,const struct omf_ledata_info_t * const info) {
    struct omf_ledata_info_t xinfo;
    unsigned long ofs = 0,len;
    long r;

    while (ofs < info->data_length) {
        if ((r=dump_LIDATA_block(fp,info,ofs,0)) < 0)
            return;

        ofs = (unsigned long)r;
    }

    if (omf_lidata_expand(info,NULL,0,&len) < 0) {
        fprintf(fp,"    [data blocks expand to more than %lu bytes]\n",(unsigned long)ULONG_MAX);
        return;
    }

    fprintf(fp,"    expanded length=0x%lX(%lu)\n",len,len);

    // hex dump the expanded data, as if it were LEDATA
    xinfo = *info;
    xinfo.iterated = 0;
    xinfo.data_length = (len > DUMP_LIDATA_MAX_EXPANDED) ? DUMP_LIDATA_MAX_EXPANDED : len;
    if (xinfo.data_length == 0)
        return;

    xinfo.data = malloc((size_t)xinfo.data_length);
    if (xinfo.data == NULL) {
        fprintf(fp,"    [not enough memory to expand data blocks]\n");
        return;
    }

    omf_lidata_expand(info,xinfo.data,xinfo.data_length,&len);
    dump_LEDATA_bytes(fp,&xinfo);
    if (len > xinfo.data_length)
        fprintf(fp,"    (first %lu bytes shown)\n",(unsigned long)xinfo.data_length);

    free(xinfo.data);
}

void dump_LIDATA(FILE *fp,const struct omf_context_t * const ctx,const struct omf_ledata_info_t * const info,const struct omf_record_t * const rec) {
    (void)rec;

    fprintf(fp,"LIDATA segment=\"%s\"(%u) data_offset=0x%lX(%lu) clength=0x%lX(%lu)\n",
        omf_context_get_segdef_name_safe(ctx,info->segment_index),
        info->segment_index,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->enum_data_offset,
        (unsigned long)info->data_length,
        (unsigned long)info->data_length);

    dump_LIDATA_blocks(fp,info);
}

