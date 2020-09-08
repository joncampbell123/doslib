
#include <stdint.h>

struct dumbpack {
    int                 fd;
    uint32_t*           offset;
    unsigned int        offset_count;
};

void dumbpack_close(struct dumbpack **d);
struct dumbpack *dumbpack_open(const char *path);
uint32_t dumbpack_ent_offset(struct dumbpack *p,unsigned int x);
uint32_t dumbpack_ent_size(struct dumbpack *p,unsigned int x);

