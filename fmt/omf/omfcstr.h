
#ifndef _DOSLIB_OMF_OMFCSTR_H
#define _DOSLIB_OMF_OMFCSTR_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

void cstr_free(char ** const p);
int cstr_set_n(char ** const p,const char * const str,const size_t strl);
 
#endif //_DOSLIB_OMF_OMFCSTR_H

