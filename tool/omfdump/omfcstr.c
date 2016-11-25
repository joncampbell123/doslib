
#include "omfcstr.h"

void cstr_free(char ** const p) {
    if (*p != NULL) {
        free(*p);
        *p = NULL;
    }
}

int cstr_set_n(char ** const p,const char * const str,const size_t strl) {
    char *x;

    cstr_free(p);

    x = malloc(strl+1);
    if (x == NULL) return -1;

    memcpy(x,str,strl);
    x[strl] = 0;

    *p = x;
    return 0;
}

