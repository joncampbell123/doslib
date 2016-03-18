
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>
#include <hw/8254/8254.h>		/* 8254 timer */

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
	char tmp[128];

	probe_dos();
	detect_windows();

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

	if (probe_dosbox_id_version_string(tmp,sizeof(tmp)))
		printf("DOSBox version string: '%s'\n",tmp);
	else
		printf("DOSBox version string N/A\n");

	dosbox_id_debug_message("This is a debug message\n");
	dosbox_id_debug_message("This is a multi-line debug message\n(second line here)\n");
	return 0;
}

