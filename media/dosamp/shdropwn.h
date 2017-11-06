
#if defined(TARGET_WINDOWS)
#include <shellapi.h>

static char *shell_queryfile(HDROP hDrop,UINT i);
unsigned int shell_dropfileshandler(HDROP hDrop);
#endif

