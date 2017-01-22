
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <hw/dos/exehdr.h>
#include <hw/dos/exenehdr.h>
#include <hw/dos/exenepar.h>

struct exe_ne_header_name_entry_table *ne_name_entry_sort_by_table = NULL;

int ne_name_entry_sort_by_name(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    unsigned char *pa,*pb;
    unsigned int i;

    pa = ne_name_entry_get_name_base(ne_name_entry_sort_by_table,ea);
    pb = ne_name_entry_get_name_base(ne_name_entry_sort_by_table,eb);

    for (i=0;i < ea->length && i < eb->length;i++) {
        int diff = (int)pa[i] - (int)pb[i];
        if (diff != 0) return diff;
    }

    if (ea->length == eb->length)
        return 0;
    else if (i < ea->length)
        return (int)pa[i];
    else /*if (i < eb->length)*/
        return 0 - (int)pb[i];
}

int ne_name_entry_sort_by_ordinal(const void *a,const void *b) {
    struct exe_ne_header_name_entry *ea = (struct exe_ne_header_name_entry *)a;
    struct exe_ne_header_name_entry *eb = (struct exe_ne_header_name_entry *)b;
    return ne_name_entry_get_ordinal(ne_name_entry_sort_by_table,ea) -
        ne_name_entry_get_ordinal(ne_name_entry_sort_by_table,eb);
}

