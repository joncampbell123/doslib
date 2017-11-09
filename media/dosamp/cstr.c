
#include <string.h>
#include <stdlib.h>

#include "cstr.h"

void free_cstr(char **str) {
    if (*str != NULL) {
        free(*str);
        *str = NULL;
    }
}

int set_cstr(char **str,const char *src) {
    free_cstr(str);

    if (src != NULL)
        *str = strdup(src);

    return (*str != NULL);
}

