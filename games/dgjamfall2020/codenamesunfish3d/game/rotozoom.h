
#include <stdint.h>

unsigned rotozoomer_imgalloc(void);
void rotozoomer_imgfree(unsigned *s);

void rotozoomer_fast_effect(unsigned int w,unsigned int h,__segment imgseg,const uint32_t rt);
int rotozoomerpngload(unsigned rotozoomerimgseg,const char *path,unsigned char palofs);

