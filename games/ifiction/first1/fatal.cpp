
#if defined(USE_WIN32)
# include <windows.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>

#include "ifict.h"
#include "utils.h"
#include "debug.h"
#include "fatal.h"

void IFEFatalError(const char *msg,...) {
	va_list va;

#if defined(USE_DOSLIB)
	IFE_win95_tf_hang_check();
#endif

	va_start(va,msg);
	vsnprintf(fatal_tmp,sizeof(fatal_tmp)/*includes NUL byte*/,msg,va);
	va_end(va);

	ifeapi->ShutdownVideo();

#if defined(USE_WIN32)
	MessageBox(NULL/*FIXME*/,fatal_tmp,"Fatal error",MB_OK|MB_ICONEXCLAMATION);
#else
	fprintf(stderr,"Fatal error: %s\n",fatal_tmp);
#endif
	exit(127);
}

