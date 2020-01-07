
#include <fmt/omf/omf.h>

int omf_context_record_write_fd(const int ofd,const struct omf_record_t * const rec) {
    unsigned char tmp[3];

    tmp[0] = rec->rectype;
    *((uint16_t*)(tmp+1)) = htole16((uint16_t)(rec->reclen + 1U)); // +1 checksum

    if (write(ofd,tmp,3) != 3)
        return -1;

    // assume the checksum byte is set as intended.
    // caller must call an "update checksum" function otherwise.
    // reclen does NOT include checksum byte.
    if (write(ofd,rec->data,rec->reclen + 1U) != (rec->reclen + 1U))
        return -1;

    return 0;
}

