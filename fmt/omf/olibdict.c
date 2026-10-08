
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

// The .LIB dictionary is library_dict_blocks blocks of OMF_LIBDICT_BLOCK_SIZE bytes at library_dict_offset.
// Each block is:
//   Buckets (OMF_LIBDICT_BUCKETS bytes)    word offset (byte offset / 2) in the block of an entry, 0 if empty
//   Free space (byte)                      word offset of the free space in the block, 0xFF if the block is full
//   Entries                                symbol name (length byte, then the name), the page number (word) of
//                                          the module that defines it, then a pad byte if needed to make it even
// The module is at file offset page number * library_block_size. Module names are entries too, ending in '!'.

// read dictionary block i into blk (OMF_LIBDICT_BLOCK_SIZE bytes). returns 0, or -1 on error.
int omf_lib_dict_read_block(unsigned char * const blk,const struct omf_context_t * const ctx,const int fd,const unsigned int i) {
    unsigned long ofs;

    if (ctx->library_dict_blocks == 0 || i >= ctx->library_dict_blocks) {
        errno = ERANGE;
        return -1;
    }

    ofs = ctx->library_dict_offset + ((unsigned long)i * (unsigned long)OMF_LIBDICT_BLOCK_SIZE);
    if (lseek(fd,(off_t)ofs,SEEK_SET) != (off_t)ofs)
        return -1;
    if (read(fd,blk,OMF_LIBDICT_BLOCK_SIZE) != OMF_LIBDICT_BLOCK_SIZE) {
        errno = EIO;
        return -1;
    }

    return 0;
}

// get the entry that bucket b of block blk points to. The name goes into name (at least 256 bytes).
// returns 1 if there is an entry, 0 if the bucket is empty, or -1 if the entry does not fit in the block.
int omf_lib_dict_get_entry(const unsigned char * const blk,const unsigned int b,char * const name,unsigned int * const page) {
    unsigned int ofs,len;

    if (b >= OMF_LIBDICT_BUCKETS)
        return -1;
    if (blk[b] == 0)
        return 0;

    ofs = (unsigned int)blk[b] * 2u;
    if (ofs < (OMF_LIBDICT_BUCKETS + 1u) || ofs >= OMF_LIBDICT_BLOCK_SIZE)
        return -1;

    len = blk[ofs++];
    if ((ofs + len + 2u) > OMF_LIBDICT_BLOCK_SIZE)
        return -1;

    memcpy(name,blk + ofs,len);
    name[len] = 0;
    ofs += len;

    *page = (unsigned int)blk[ofs] | ((unsigned int)blk[ofs+1] << 8u);
    return 1;
}

