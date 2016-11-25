
#ifndef _DOSLIB_OMF_OMFREC_H
#define _DOSLIB_OMF_OMFREC_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

//================================== OMF rec types ================================

#define OMF_RECTYPE_THEADR      (0x80)

#define OMF_RECTYPE_LNAMES      (0x96)

#define OMF_RECTYPE_SEGDEF      (0x98)
#define OMF_RECTYPE_SEGDEF32    (0x99)

#define OMF_RECTYPE_GRPDEF      (0x9A)
#define OMF_RECTYPE_GRPDEF32    (0x9B)

//================================== records ================================

struct omf_record_t {
    unsigned char           rectype;
    unsigned short          reclen;             // amount of data in data, reclen < data_alloc not including checksum
    unsigned short          recpos;             // read/write position
    unsigned char*          data;               // data if != NULL. checksum is at data[reclen]
    size_t                  data_alloc;         // amount of data allocated if data != NULL or amount TO alloc if data == NULL

    unsigned long           rec_file_offset;    // file offset of record (~0UL if undefined)
};

unsigned char omf_record_is_modend(const struct omf_record_t * const rec);
void omf_record_init(struct omf_record_t * const rec);
void omf_record_data_free(struct omf_record_t * const rec);
int omf_record_data_alloc(struct omf_record_t * const rec,size_t sz);
void omf_record_clear(struct omf_record_t * const rec);
unsigned short omf_record_lseek(struct omf_record_t * const rec,unsigned short pos);
size_t omf_record_can_write(const struct omf_record_t * const rec);
size_t omf_record_data_available(const struct omf_record_t * const rec);
unsigned char omf_record_get_byte_fast(struct omf_record_t * const rec);
unsigned char omf_record_get_byte(struct omf_record_t * const rec);
unsigned short omf_record_get_word_fast(struct omf_record_t * const rec);
unsigned short omf_record_get_word(struct omf_record_t * const rec);
unsigned long omf_record_get_dword_fast(struct omf_record_t * const rec);
unsigned long omf_record_get_dword(struct omf_record_t * const rec);
unsigned int omf_record_get_index(struct omf_record_t * const rec);
int omf_record_read_data(unsigned char *dst,unsigned int len,struct omf_record_t *rec);
int omf_record_get_lenstr(char *dst,const size_t dstmax,struct omf_record_t *rec);
void omf_record_free(struct omf_record_t * const rec);

static inline unsigned char omf_record_eof(const struct omf_record_t * const rec) {
    return (omf_record_data_available(rec) == 0);
}

#endif //_DOSLIB_OMF_OMFREC_H

