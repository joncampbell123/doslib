
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

int16_t *buffer = NULL;
size_t buffer_size = 0;
uint32_t buffer_phys = 0;

uint32_t sample_rate = 0;
uint16_t channel_count = 0;

int main(int argc,char **argv,char **envp) {
	(void)argc;
	(void)argv;
	(void)envp;

	probe_dos();
    cpu_probe();
	detect_windows();

    if (windows_mode == WINDOWS_NT) {
        printf("This program is not compatible with Windows NT\n");
        return 1;
    }
    if (cpu_flags & CPU_FLAG_V86_ACTIVE) {
        printf("This program cannot be safely used in virtual 8086 mode\n");
        return 1;
    }

#if TARGET_MSDOS == 32
    /* if we can't use the LTP library, then we can't do the trick */
    if (!dos_ltp_probe()) {
        printf("Cannot determine physical memory mapping\n");
        return 1;
    }
    if (dos_ltp_info.paging) { // DOS extenders and EMM386 may remap DMA to make it work but bus mastering... no way
        printf("Cannot determine physical memory mapping\n");
        return 1;
    }
#endif

	if (!probe_dosbox_id()) {
		printf("DOSBox integration device not found\n");
		return 1;
	}
	printf("DOSBox integration device found at I/O port %xh\n",dosbox_id_baseio);

#if TARGET_MSDOS == 32
    buffer_size = 512 * 1024;
    buffer = malloc(buffer_size); // CAUTION: Assumes EMM386.EXE does not have the system in virtual 8086 mode
    buffer_phys = (uint32_t)buffer;
    printf("Buffer: %p phys 0x%08lx\n",buffer,buffer_phys);
#else
    buffer_size = 16384;
    buffer = malloc(buffer_size);
    buffer_phys = ((unsigned long)FP_SEG(buffer) << 4ul) + (unsigned long)FP_OFF(buffer);
    printf("Buffer: %Fp phys 0x%08lx\n",(unsigned char far*)buffer,buffer_phys);
#endif
    if (buffer == NULL) {
        printf("Malloc failed\n");
        return 1;
    }
    memset(buffer,0,buffer_size);

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

        sample_rate = (uint32_t)(mixq & 0xFFFFFul);
        channel_count = (uint16_t)((mixq >> 20ul) & 0xFul);

        if (sample_rate == 0 || channel_count == 0 || channel_count > 8) {
            printf("Cannot accept mixer params\n");
            return 1;
        }
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
        unsigned char tmp[44];
        uint32_t c,sz;
        int fd;

        printf("* Start means the first byte of memory\n");
        printf("* End means the first byte past the end of the buffer\n");

        printf("Buffer start: 0x%08lx\n",(unsigned long)buffer_phys);
        printf("Buffer end:   0x%08lx\n",(unsigned long)buffer_phys + buffer_size);

        dosbox_id_write_regsel(DOSBOX_ID_REG_MIXER_WRITEPOS);
        c = dosbox_id_read_data();
        printf("Finished: Write pos 0x%08lx\n",(unsigned long)c);

        {
            sz = c - buffer_phys;
            sz -= sz % (size_t)(2 * channel_count);
            if (sz <= buffer_size && sz > 0) {
                /* prove it's working by writing the audio to disk */
                fd = open("MIXC.WAV",O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,0644);
                if (fd >= 0) {
                    memcpy(tmp+0x00,"RIFF",4);
                    *((uint32_t*)(tmp+0x04)) = (sz + 44u - 8u);
                    memcpy(tmp+0x08,"WAVE",4);

                    memcpy(tmp+0x0C,"fmt ",4); // struct starts at 0x14, 16 bytes
                    *((uint32_t*)(tmp+0x10)) = 16;
                    *((uint16_t*)(tmp+0x14)) = 0x0001;                              // WAVE_FORMAT_PCM
                    *((uint16_t*)(tmp+0x16)) = channel_count;                       // nChannels
                    *((uint32_t*)(tmp+0x18)) = sample_rate;                         // nSamplesPerSec
                    *((uint32_t*)(tmp+0x1C)) = sample_rate * channel_count * 2ul;   // nAvgBytesPerSec
                    *((uint16_t*)(tmp+0x20)) = 2 * channel_count;                   // nBlockAlign
                    *((uint16_t*)(tmp+0x22)) = 16;                                  // wBitsPerSample

                    memcpy(tmp+0x24,"data",4); // data starts at 0x2C (44)
                    *((uint32_t*)(tmp+0x28)) = sz;

                    write(fd,tmp,0x2C);
                    write(fd,buffer,sz);

                    close(fd);
                }
            }
        }
    }

    return 0;
}

