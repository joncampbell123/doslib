
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_next_lib_module_fd(struct omf_context_t * const ctx,int fd) {
    unsigned long ofs;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was not a MODEND, then stop reading.
    if ((ctx->record.rectype&0xFE) != 0x8A) { // Not 0x8A or 0x8B
        errno = EIO;
        return -1;
    }

    // if we don't have a block size, then we cannot advance
    if (ctx->library_block_size == 0UL)
        return 0;

    // where does the next block size start?
    ofs = ctx->record.rec_file_offset + 3 + ctx->record.reclen;
    ofs += ctx->library_block_size - 1UL;
    ofs -= ofs % ctx->library_block_size;
    if (lseek(fd,(off_t)ofs,SEEK_SET) != (off_t)ofs)
        return 0;

    ctx->record.rec_file_offset = ofs;
    ctx->record.rectype = 0;
    ctx->record.reclen = 0;
    return 1;
}

