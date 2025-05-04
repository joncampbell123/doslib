
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <malloc.h>
#include <ctype.h>
#include <fcntl.h>
#include <dos.h>

#include <hw/dos/dos.h>
#include <hw/dos/doswin.h>

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int main(int argc,char **argv,char **envp) {
    unsigned int raise=0;
    unsigned int irq;

	(void)argc;
	(void)argv;
	(void)envp;

    if (argc < 3) {
        printf("IRQ <irq> <raise/lower>\n");
        return 1;
    }
    irq = (unsigned int)strtoul(argv[1],NULL,0);
    if (irq > 15) return 1;

    if (!strcasecmp(argv[2],"raise") || !strcmp(argv[2],"1"))
        raise=1;

	probe_dos();
	detect_windows();

    if (windows_mode == WINDOWS_NT) {
        printf("This program is not compatible with Windows NT\n");
        return 1;
    }

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

    dosbox_id_write_regsel(DOSBOX_ID_REG_8259_INJECT_IRQ);
    dosbox_id_write_data(((uint32_t)raise << (uint32_t)8u) + irq);

    return 0;
}

