
#if defined(TARGET_WINDOWS)
# define HW_DOS_DONT_DEFINE_MMSYSTEM
# include <windows.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

#include "dosamp.h"
#include "fs.h"

#if defined(LINUX)
/* case sensitive */
int fs_comparechar(char c1,char c2) {
    return (c1 == c2);
}
#else
/* case insensitive */
int fs_comparechar(char c1,char c2) {
    return (tolower(c1) == tolower(c2));
}
#endif

