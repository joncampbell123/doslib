
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_read_fd(struct omf_context_t * const ctx,int fd) {
    unsigned char sum = 0;
    unsigned char tmp[3];
    unsigned int i;
    int ret;

    // if the last record was a LIBEND, then stop reading.
    // non-OMF junk usually follows.
    if (ctx->record.rectype == 0xF1)
        return 0;

    // if the last record was a MODEND, then stop reading, make caller move to next module with another function
    if ((ctx->record.rectype&0xFE) == 0x8A) // 0x8A or 0x8B
        return 0;

    ctx->last_error = NULL;
    omf_record_clear(&ctx->record);
    if (ctx->record.data == NULL && omf_record_data_alloc(&ctx->record,0) < 0)
        return -1; // sets errno
    if (ctx->record.data_alloc < 16) {
        ctx->last_error = "Record buffer too small";
        errno = EINVAL;
        return -1;
    }

    ctx->record.rec_file_offset = lseek(fd,0,SEEK_CUR);

    if ((ret=read(fd,tmp,3)) != 3) {
        if (ret >= 0) {
            return 0; // EOF
        }

        ctx->last_error = "Reading OMF record header failed";
        // read sets errno
        return -1;
    }
    ctx->record.rectype = tmp[0];
    ctx->record.reclen = *((uint16_t*)(tmp+1)); // length (including checksum)
    if (ctx->record.rectype == 0 || ctx->record.reclen == 0)
        return 0;
    if (ctx->record.reclen > ctx->record.data_alloc) {
        ctx->last_error = "Reading OMF record failed because record too large for buffer";
        errno = ERANGE;
        return -1;
    }
    if ((unsigned int)(ret=read(fd,ctx->record.data,ctx->record.reclen)) != (unsigned int)ctx->record.reclen) {
        ctx->last_error = "Reading OMF record contents failed";
        if (ret >= 0) errno = EIO;
        return -1;
    }

    /* check checksum */
    if (ctx->record.data[ctx->record.reclen-1] != 0/*optional*/) {
        for (i=0;i < 3;i++)
            sum += tmp[i];
        for (i=0;i < ctx->record.reclen;i++)
            sum += ctx->record.data[i];

        if (sum != 0) {
            ctx->last_error = "Reading OMF record checksum failed";
            errno = EIO;
            return -1;
        }
    }

    /* remember LIBHEAD block size */
    if (ctx->record.rectype == 0xF0/*LIBHEAD*/) {
        if (ctx->library_block_size == 0) {
            // and the length of the record defines the block size that modules within are aligned by
            ctx->library_block_size = ctx->record.reclen + 3;
        }
        else {
            ctx->last_error = "LIBHEAD defined again";
            errno = EIO;
            return -1;
        }
    }

    ctx->record.reclen--; // omit checksum from reclen
    return 1;
}

