
#include <fmt/omf/omf.h>
#include <fmt/omf/omfcstr.h>

int omf_context_read_fd(struct omf_context_t * const ctx,int fd) {
    unsigned char checksum;
    unsigned char sum = 0;
    unsigned char tmp[3];
    unsigned int readlen;
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
    ctx->record.reclen = le16toh(*((uint16_t*)(tmp+1))); // length (including checksum)
    if (ctx->record.rectype == 0 || ctx->record.reclen == 0)
        return 0;

    readlen = ctx->record.reclen;
    if (readlen > ctx->record.data_alloc) {
        // LIBHEAD is padded out to the .LIB page size, which can be larger than the
        // record buffer. Only the start of it means anything, so read what fits
        // (leaving room for the checksum byte) and read through the rest below.
        if (ctx->record.rectype != 0xF0/*LIBHEAD*/) {
            ctx->last_error = "Reading OMF record failed because record too large for buffer";
            errno = ERANGE;
            return -1;
        }

        readlen = ctx->record.data_alloc - 1;
    }
    if ((unsigned int)(ret=read(fd,ctx->record.data,readlen)) != readlen) {
        ctx->last_error = "Reading OMF record contents failed";
        if (ret >= 0) errno = EIO;
        return -1;
    }

    for (i=0;i < 3;i++)
        sum += tmp[i];
    for (i=0;i < readlen;i++)
        sum += ctx->record.data[i];

    checksum = ctx->record.data[readlen-1];

    // read through the rest of an oversized LIBHEAD
    if (readlen < ctx->record.reclen) {
        unsigned int skip = ctx->record.reclen - readlen;
        unsigned char skipbuf[64];
        unsigned int n;

        while (skip > 0) {
            n = (skip > sizeof(skipbuf)) ? sizeof(skipbuf) : skip;
            if ((unsigned int)(ret=read(fd,skipbuf,n)) != n) {
                ctx->last_error = "Reading OMF record contents failed";
                if (ret >= 0) errno = EIO;
                return -1;
            }

            for (i=0;i < n;i++)
                sum += skipbuf[i];

            checksum = skipbuf[n-1];
            skip -= n;
        }
    }

    /* check checksum */
    if (checksum != 0/*optional*/ && sum != 0) {
        ctx->last_error = "Reading OMF record checksum failed";
        errno = EIO;
        return -1;
    }

    /* remember LIBHEAD block size */
    if (ctx->record.rectype == 0xF0/*LIBHEAD*/) {
        if (ctx->library_block_size == 0) {
            // and the length of the record defines the block size that modules within are aligned by
            ctx->library_block_size = ctx->record.reclen + 3;

            // if we only read the start of the LIBHEAD, then that's all the record holds (no checksum)
            if (readlen < ctx->record.reclen) {
                ctx->record.reclen = readlen;
                return 1;
            }
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

