
#if !defined(TARGET_WINDOWS) && TARGET_MSDOS == 16

#ifndef __HW_DOS_EXELOAD_H
#define __HW_DOS_EXELOAD_H

# include <dos.h>
# include <stdio.h>
# include <fcntl.h>
# include <stdint.h>
# include <string.h>
# include <stdlib.h>
# include <unistd.h>

# include <hw/dos/exehdr.h>

struct exeload_ctx {
    unsigned int                base_seg;           // base segment
    unsigned int                len_seg;            // length in segments (paragraphs)
    struct exe_dos_header*      header;             // allocated copy of EXE header. you can free to conserve memory.
};

#define exeload_ctx_INIT {0,0,NULL}

void exeload_free(struct exeload_ctx *exe);
void exeload_free_header(struct exeload_ctx *exe);
int exeload_load(struct exeload_ctx *exe,const char * const path);
int exeload_load_fd(struct exeload_ctx *exe,const int fd/*must be open, you must lseek to EXE header*/);

static inline unsigned long exeload_resident_length(const struct exeload_ctx * const exe) {
    return (unsigned long)exe->len_seg << 4UL;
}

#endif //__HW_DOS_EXELOAD_H

#endif

