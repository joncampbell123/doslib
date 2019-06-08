
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

#include <hw/dosboxid/iglib.h>

#ifdef TARGET_WINDOWS
# define WINFCON_STOCK_WIN_MAIN
# include <hw/dos/winfcon.h>
#endif

int16_t *buffer = NULL;
size_t buffer_size = 0;
uint32_t buffer_phys = 0;

int main(int argc,char **argv,char **envp) {
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

#if TARGET_MSDOS == 32
    buffer_size = 512 * 1024;
    buffer = malloc(buffer_size); // CAUTION: Assumes EMM386.EXE does not have the system in virtual 8086 mode
    buffer_phys = (uint32_t)buffer;
#else
    buffer_size = 16384;
    buffer = malloc(buffer_size);
    buffer_phys = (FP_SEG(buffer) << 4ul) + FP_OFF(buffer);
#endif
    if (buffer == NULL) {
        printf("Malloc failed\n");
        return 1;
    }

    {
        uint32_t mixq;

		dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_QUERY);
		mixq = dosbox_id_read_data();

        printf("Mixer: %u-channel %luHz mute=%u sound=%u swapstereo=%u\n",
            (unsigned int)((mixq >> 20ul) & 0xFul),
            (unsigned long)(mixq & 0xFFFFFul),
            (unsigned int)((mixq >> 30ul) & 1ul),
            ((unsigned int)((mixq >> 31ul) & 1ul)) ^ 1,
            (unsigned int)((mixq >> 29ul) & 1ul));
    }

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_CONTROL);
    dosbox_id_write_data(0); // turn off and reset mixer recording control

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_WRITEPOS);
    dosbox_id_write_data(buffer_phys);

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_WRITEBEGIN);
    dosbox_id_write_data(buffer_phys);

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_WRITEEND); // first byte past end of buffer
    dosbox_id_write_data(buffer_phys + buffer_size);

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_CONTROL);
    dosbox_id_write_data(0x3); // enable capture and writing to memory

    printf("Recording...\n");
    do {
        uint32_t c;

        dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_CONTROL);
        c = dosbox_id_read_data();
        if ((c & 3) != 3) {
            if (c & 8) printf("It stopped, error (0x%08lx)\n",(unsigned long)c);
            else printf("It stopped normally\n",(unsigned long)c);
            break;
        }

        if (kbhit()) {
            int c = getch();
            if (c == 27) {
                printf("User exit\n");
                break;
            }
        }
    } while(1);

    dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_CONTROL);
    dosbox_id_write_data(0); // turn off and reset mixer recording control

    {
        uint32_t c;

        printf("* Start means the first byte of memory\n");
        printf("* End means the first byte past the end of the buffer\n");

        printf("Buffer start: 0x%08lx\n",(unsigned long)buffer_phys);
        printf("Buffer end:   0x%08lx\n",(unsigned long)buffer_phys + buffer_size);

        dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_WRITEPOS);
        c = dosbox_id_read_data();
        printf("Finished: Write pos 0x%08lx\n",(unsigned long)c);
    }

    return 0;
}

