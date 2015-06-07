
#include <stdint.h>

#if !defined(TARGET_WINDOWS) && !defined(TARGET_OS2)
# define HAVE_GUS_MEGAEM_DETECT
struct mega_em_info {
    unsigned char	intnum;
    uint16_t		version;
    uint16_t		response;
};

extern struct mega_em_info megaem_info;

int gravis_mega_em_detect(struct mega_em_info *x);
#endif

