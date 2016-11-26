
#include <fmt/omf/omf.h>

unsigned char omf_record_is_modend(const struct omf_record_t * const rec) {
    return ((rec->rectype&0xFE) == 0x8A); // MODEND 0x8A or 0x8B
}

void omf_record_init(struct omf_record_t * const rec) {
    rec->rectype = 0;
    rec->reclen = 0;
    rec->data = NULL;
    rec->data_alloc = 4096; // OMF spec says 1024
    rec->rec_file_offset = (~0UL);
}

void omf_record_data_free(struct omf_record_t * const rec) {
    if (rec->data != NULL) {
        free(rec->data);
        rec->data = NULL;
    }
    rec->reclen = 0;
    rec->rectype = 0;
}

int omf_record_data_alloc(struct omf_record_t * const rec,size_t sz) {
    if (sz > (size_t)0xFF00U) {
        errno = ERANGE;
        return -1;
    }

    if (rec->data != NULL) {
        if (sz == 0 || sz == rec->data_alloc)
            return 0;

        errno = EINVAL;//cannot realloc
        return -1;
    }

    if (sz == 0) // default?
        sz = 4096;

    rec->data = malloc(sz);
    if (rec->data == NULL)
        return -1; // malloc sets errno

    rec->data_alloc = sz;
    rec->reclen = 0;
    rec->recpos = 0;
    return 0;
}

void omf_record_clear(struct omf_record_t * const rec) {
    rec->recpos = 0;
    rec->reclen = 0;
}

unsigned short omf_record_lseek(struct omf_record_t * const rec,unsigned short pos) {
    if (rec->data == NULL)
        rec->recpos = 0;
    else if (pos > rec->reclen)
        rec->recpos = rec->reclen;
    else
        rec->recpos = pos;

    return rec->recpos;
}

size_t omf_record_can_write(const struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0;
    if (rec->recpos >= rec->data_alloc)
        return 0;

    return (size_t)(rec->data_alloc - rec->recpos);
}

size_t omf_record_data_available(const struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0;
    if (rec->recpos >= rec->reclen)
        return 0;

    return (size_t)(rec->reclen - rec->recpos);
}

unsigned char omf_record_get_byte_fast(struct omf_record_t * const rec) {
    unsigned char c;

    c = *((unsigned char*)(rec->data + rec->recpos));
    rec->recpos++;
    return c;
}

unsigned char omf_record_get_byte(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFU;
    if ((rec->recpos+1U) > rec->reclen)
        return 0xFFU;

    return omf_record_get_byte_fast(rec);
}

unsigned short omf_record_get_word_fast(struct omf_record_t * const rec) {
    unsigned short c;

    c = *((uint16_t*)(rec->data + rec->recpos));
    rec->recpos += 2;
    return c;
}

unsigned short omf_record_get_word(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFFFU;
    if ((rec->recpos+2U) > rec->reclen)
        return 0xFFFFU;

    return omf_record_get_word_fast(rec);
}

unsigned long omf_record_get_dword_fast(struct omf_record_t * const rec) {
    unsigned long c;

    c = *((uint32_t*)(rec->data + rec->recpos));
    rec->recpos += 4;
    return c;
}

unsigned long omf_record_get_dword(struct omf_record_t * const rec) {
    if (rec->data == NULL)
        return 0xFFFFU;
    if ((rec->recpos+4U) > rec->reclen)
        return 0xFFFFU;

    return omf_record_get_dword_fast(rec);
}

unsigned int omf_record_get_index(struct omf_record_t * const rec) {
    unsigned int t;

    /* 1 or 2 bytes.
       1 byte if less than 0x80
       2 bytes if more than 0x7F
       
       1 byte encoding (0x00-0x7F)  (value & 0x7F) 
       2 byte encoding (>0x80)      ((value & 0x7F) | 0x80) (value >> 8)*/
    if (omf_record_eof(rec)) return 0;
    t = omf_record_get_byte_fast(rec);
    if (t & 0x80) {
        t = (t & 0x7F) << 8U;
        if (omf_record_eof(rec)) return 0;
        t += omf_record_get_byte_fast(rec);
    }

    return t;
}

int omf_record_read_data(unsigned char *dst,unsigned int len,struct omf_record_t *rec) {
    unsigned int avail;

    avail = omf_record_data_available(rec);
    if (len > avail) len = avail;

    if (len != 0) {
        memcpy(dst,rec->data+rec->recpos,len);
        rec->recpos += len;
    }

    return len;
}

int omf_record_get_lenstr(char *dst,const size_t dstmax,struct omf_record_t *rec) {
    unsigned char len;

    len = omf_record_get_byte(rec);
    if ((len+1U) > dstmax) {
        errno = ENOSPC;
        return -1;
    }

    if (omf_record_read_data((unsigned char*)dst,len,rec) < len) {
        errno = EIO;
        return -1;
    }

    dst[len] = 0;
    return len;
}

void omf_record_free(struct omf_record_t * const rec) {
    omf_record_data_free(rec);
}

