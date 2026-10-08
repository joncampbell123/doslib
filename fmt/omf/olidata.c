
#include <limits.h>

#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

// LIDATA iterated data block (also the data of a COMDAT with the iterated flag)
//   Repeat count (word, or dword in a 32-bit record)
//   Block count (word)
//   Content: if block count == 0, a byte count then that many bytes, else block count nested data blocks
//
// read the header of the data block at data offset ofs. returns 0, or -1 if it does not fit in the data.
int omf_lidata_get_block(struct omf_lidata_block_t * const blk,const struct omf_ledata_info_t * const info,const unsigned long ofs) {
    const unsigned long need = info->is32bit ? 6ul : 4ul;
    const unsigned char *p;

    if (info->data == NULL || ofs > info->data_length || (info->data_length - ofs) < need)
        return -1;

    p = info->data + ofs;
    if (info->is32bit) {
        blk->repeat_count = omf_le32(p);
        p += 4;
    }
    else {
        blk->repeat_count = omf_le16(p);
        p += 2;
    }

    blk->block_count = omf_le16(p);
    blk->content_offset = ofs + need;
    blk->content_length = 0;

    if (blk->block_count == 0) {
        if (blk->content_offset >= info->data_length)
            return -1;

        blk->content_length = info->data[blk->content_offset++];
        if ((info->data_length - blk->content_offset) < blk->content_length)
            return -1;
    }

    return 0;
}

// expand the data block at data offset ofs, and the blocks nested in it.
// returns the data offset after the block, or -1 if it is malformed.
static long omf_lidata_expand_block(const struct omf_ledata_info_t * const info,unsigned long ofs,unsigned char * const dst,const unsigned long dstmax,unsigned long * const len,const unsigned int depth) {
    struct omf_lidata_block_t blk;
    unsigned long start = *len,one,rep,i;
    long r;

    if (depth >= OMF_LIDATA_MAX_DEPTH)
        return -1;
    if (omf_lidata_get_block(&blk,info,ofs) < 0)
        return -1;

    // expand the content once
    ofs = blk.content_offset;
    if (blk.block_count == 0) {
        if (blk.content_length > (ULONG_MAX - *len))
            return -1;

        for (i=0;i < blk.content_length;i++) {
            if (dst != NULL && *len < dstmax)
                dst[*len] = info->data[ofs+i];

            (*len)++;
        }

        ofs += blk.content_length;
    }
    else {
        for (i=0;i < blk.block_count;i++) {
            if ((r=omf_lidata_expand_block(info,ofs,dst,dstmax,len,depth+1)) < 0)
                return -1;

            ofs = (unsigned long)r;
        }
    }

    // then repeat it
    one = *len - start;
    if (blk.repeat_count == 0) {
        *len = start;
    }
    else if (one != 0) {
        rep = blk.repeat_count - 1;
        if (rep > ((ULONG_MAX - *len) / one))
            return -1;

        // copy the repeats that fit in dst, then count the rest
        while (rep > 0 && dst != NULL && *len < dstmax) {
            for (i=0;i < one;i++) {
                if (*len < dstmax)
                    dst[*len] = dst[start+i];

                (*len)++;
            }

            rep--;
        }

        *len += rep * one;
    }

    return (long)ofs;
}

// expand the iterated data blocks of an LIDATA record, or of a COMDAT with the iterated flag.
// *len gets the expanded length, and up to dstmax bytes of the expanded data go into dst (if not NULL).
// returns 0, or -1 if the blocks are malformed, nested too deeply, or expand to more than ULONG_MAX bytes.
int omf_lidata_expand(const struct omf_ledata_info_t * const info,unsigned char * const dst,const unsigned long dstmax,unsigned long * const len) {
    unsigned long ofs = 0;
    long r;

    *len = 0;
    while (ofs < info->data_length) {
        if ((r=omf_lidata_expand_block(info,ofs,dst,dstmax,len,0)) < 0)
            return -1;

        ofs = (unsigned long)r;
    }

    return 0;
}

