
#include <fmt/omf/omf.h>

static inline void omf_record_write_byte_fast(struct omf_record_t * const rec,const unsigned char c) {
    rec->data[rec->recpos] = c;
    rec->recpos++;
}

int omf_record_write_byte(struct omf_record_t * const rec,const unsigned char c) {
    if (rec->data == NULL)
        return -1;
    if ((rec->recpos+1U+1U) > rec->data_alloc)
        return -1;

    omf_record_write_byte_fast(rec,c);
    return 0;
}

static inline void omf_record_write_word_fast(struct omf_record_t * const rec,const unsigned short c) {
    *((uint16_t*)(rec->data+rec->recpos)) = c;
    rec->recpos += 2;
}

int omf_record_write_word(struct omf_record_t * const rec,const unsigned short c) {
    if (rec->data == NULL)
        return -1;
    if ((rec->recpos+1U+2U) > rec->data_alloc)
        return -1;

    omf_record_write_word_fast(rec,c);
    return 0;
}

static inline void omf_record_write_dword_fast(struct omf_record_t * const rec,const unsigned long c) {
    *((uint32_t*)(rec->data+rec->recpos)) = c;
    rec->recpos += 4;
}

int omf_record_write_dword(struct omf_record_t * const rec,const unsigned long c) {
    if (rec->data == NULL)
        return -1;
    if ((rec->recpos+1U+4U) > rec->data_alloc)
        return -1;

    omf_record_write_dword_fast(rec,c);
    return 0;
}

int omf_record_write_index(struct omf_record_t * const rec,const unsigned short c) {
    if (rec->data == NULL)
        return -1;
    if ((rec->recpos+1U+2U) > rec->data_alloc)
        return -1;

    if (c >= 0x80) {
        omf_record_write_byte_fast(rec,0x80 + ((c >> 8) & 0x7F));
        omf_record_write_byte_fast(rec,c & 0xFF);
    }
    else {
        omf_record_write_byte_fast(rec,c);
    }

    return 0;
}

void omf_record_write_update_reclen(struct omf_record_t * const rec) {
    rec->reclen = rec->recpos;
}

void omf_record_write_update_checksum(struct omf_record_t * const rec) {
    unsigned char sum = 0;
    unsigned short l = rec->reclen + 1U;
    unsigned int i;

    sum += rec->rectype;
    sum += (l & 0xFF);
    sum += (l >> 8);
    for (i=0;i < rec->reclen;i++)
        sum += rec->data[i];

    rec->data[rec->reclen] = 0x100 - sum;
}

