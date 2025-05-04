
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
	(void)argc;
	(void)argv;
	(void)envp;

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

    dosbox_id_write_regsel(DOSBOX_ID_REG_8259_PIC_INFO);
    {
        uint32_t t = dosbox_id_read_data();

        if ((t&0xFFu) != 0xFFu)
            printf("Cascade: IRQ %u\n",t&0xFFu);
        else
            printf("Cascade: None\n");

        printf("Primary PIC: %s\n",(t&0x100u)?"present":"not present");
        printf("Secondary PIC: %s\n",(t&0x200u)?"present":"not present");
    }

    return 0;
}

