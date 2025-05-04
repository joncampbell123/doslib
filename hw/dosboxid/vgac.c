
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

#pragma pack(push,1)
typedef struct windows_BITMAPFILEHEADER {
    uint16_t  bfType;
    uint32_t  bfSize;
    uint16_t  bfReserved1;
    uint16_t  bfReserved2;
    uint32_t  bfOffBits;
} windows_BITMAPFILEHEADER;

typedef struct windows_BITMAPINFOHEADER {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} windows_BITMAPINFOHEADER;
#pragma pack(pop)

uint8_t *buffer = NULL;
uint32_t buffer_size = 0;
uint32_t buffer_phys = 0;

uint32_t buffer_max = 0;
uint32_t buffer_width = 0;
uint32_t buffer_height = 0;
uint32_t buffer_stride = 0;
uint16_t vga_width = 0,vga_height = 0;

void write_frame(void) {
    windows_BITMAPFILEHEADER fhdr;
    windows_BITMAPINFOHEADER ihdr;
    uint32_t y;
    int fd;

    fd = open("VGA.BMP",O_WRONLY|O_CREAT|O_TRUNC,0644);
    if (fd < 0) {
        fprintf(stderr,"Failed to write VGA.BMP\n");
        return;
    }

    memset(&fhdr,0,sizeof(fhdr));
    memset(&ihdr,0,sizeof(ihdr));

    fhdr.bfType = 0x4D42;       // "BM"
    fhdr.bfSize = sizeof(fhdr) + sizeof(ihdr) + (buffer_stride * buffer_height);
    fhdr.bfOffBits = sizeof(fhdr) + sizeof(ihdr);
    write(fd,&fhdr,sizeof(fhdr));

    ihdr.biSize = sizeof(ihdr); // 40
    ihdr.biWidth = buffer_stride / 4;   // stride is rounded up to DWORD multiple. Each pixel is a DWORD, therefore no rounding needed.
    ihdr.biHeight = buffer_height;
    ihdr.biPlanes = 1;
    ihdr.biBitCount = 32;
    ihdr.biCompression = 0;
    ihdr.biSizeImage = buffer_stride * buffer_height;
    write(fd,&ihdr,sizeof(ihdr));

    for (y=0;y < buffer_height;y++)
        write(fd,buffer + ((unsigned int)buffer_height-1u-y)*buffer_stride,buffer_stride);

    close(fd);
}

int main(int argc,char **argv,char **envp) {
    int x,y,w,h; // allow signed to test whether DOSBox-X validates coordinates or not
    uint32_t tmp;

	(void)argc;
	(void)argv;
	(void)envp;

    if (argc < 5) {
        printf("VGAC x y w h\n");
        return 1;
    }

    x = atoi(argv[1]);
    y = atoi(argv[2]);
    w = atoi(argv[3]);
    h = atoi(argv[4]);

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

    dosbox_id_write_regsel(DOSBOX_ID_CMD_GET_VGA_SIZE);
    tmp = dosbox_id_read_data();
    vga_width = (tmp & 0xFFFF);
    vga_height = tmp >> 16ul;

#if TARGET_MSDOS == 32
    buffer_max = 64*1024*1024;
#else
    buffer_max = 24*1024;
#endif

    buffer_stride = (uint32_t)w * 4ul; // output is always 32bpp XRGB

    printf("VGA: %u x %u\n",vga_width,vga_height);
    printf("Will capture: x=%d y=%d w=%d h=%d\n",x,y,w,h);
    printf("Buffer max: %lu\n",(unsigned long)buffer_max);
    printf("Stride: %lu\n",(unsigned long)buffer_stride);

    if (x < 0 || y < 0 || x >= vga_width || y >= vga_height)
        printf("WARNING: x and/or y is out of range\n");
    if (w < 0 || h < 0 || (unsigned int)w > vga_width || (unsigned int)h > vga_height)
        printf("WARNING: w and/or h is too large for frame\n");
    if (((unsigned int)x+(unsigned int)w) > vga_width || ((unsigned int)y+(unsigned int)h) > vga_height)
        printf("WARNING: rectangle is out of bounds\n");

    if (w <= 0 || h <= 0 || h > 4096 || w > 4096) {
        printf("Dimensions invalid\n");
        return 1;
    }
    buffer_width = (unsigned int)w;
    buffer_height = (unsigned int)h;
    buffer_size = buffer_stride * buffer_height;
    printf("Buffer size: %lu max %lu height %lu\n",(unsigned long)buffer_size,(unsigned long)buffer_max,(unsigned long)buffer_height);

    if (buffer_size > buffer_max) {
        printf("Buffer size too large\n");
        return 1;
    }

#if TARGET_MSDOS == 32
    buffer = malloc(buffer_size); // CAUTION: Assumes EMM386.EXE does not have the system in virtual 8086 mode
    buffer_phys = (uint32_t)buffer;
    printf("Buffer: %p phys 0x%08lx\n",buffer,buffer_phys);
#else
    buffer = malloc(buffer_size);
    buffer_phys = ((unsigned long)FP_SEG(buffer) << 4ul) + (unsigned long)FP_OFF(buffer);
    printf("Buffer: %Fp phys 0x%08lx\n",(unsigned char far*)buffer,buffer_phys);
#endif
    if (buffer == NULL) {
        printf("Malloc failed\n");
        return 1;
    }
    memset(buffer,0,buffer_size);

    // stop capture entirely
    {
        uint32_t tmp;

        dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STATE);
        tmp = dosbox_id_read_data() & (~DOSBOX_VGA_CAPTURE_ENABLE);

        dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STATE);
        dosbox_id_write_data(tmp); // write it back (minus enable) to clear error bits

        dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STATE);
        dosbox_id_write_data(0UL); // clear all bits
    }

    // set up capture
    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_ADDR);
    dosbox_id_write_data(buffer_phys);

    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STRIDE);
    dosbox_id_write_data(buffer_stride);

    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_POS);
    dosbox_id_write_data(((unsigned long)((unsigned int)y & 0xFFFFu) << 16ul) | ((unsigned int)x & 0xFFFFu));

    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_SIZE);
    dosbox_id_write_data(((unsigned long)((unsigned int)h & 0xFFFFu) << 16ul) | ((unsigned int)w & 0xFFFFu));

    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STATE);
    dosbox_id_write_data(DOSBOX_VGA_CAPTURE_ENABLE); // GO

    // wait for capture to complete
    printf("Capture started...\n");
    {
        uint32_t tmp;

        do {
            dosbox_id_write_regsel(DOSBOX_ID_CMD_GET_VGA_CAPTURE_STATE);
            tmp = dosbox_id_read_data();

            if (tmp & (DOSBOX_VGA_CAPTURE_ACQUIRED|DOSBOX_VGA_CAPTURE_ERROR|DOSBOX_VGA_CAPTURE_MISSED_FRAME))
                break;
        } while (1);

        printf("Capture result flags: 0x%08lx\n",(unsigned long)tmp);

        if (tmp & DOSBOX_VGA_CAPTURE_ACQUIRED) {
            printf("Frame acquired\n");
            write_frame();
        }
        else {
            printf("Frame not acquired\n");
            if (tmp & DOSBOX_VGA_CAPTURE_ERROR)
                printf("Capture error\n");
            if (tmp & DOSBOX_VGA_CAPTURE_MISSED_FRAME)
                printf("Missed frame\n");
        }
    }

    // stop capture
    dosbox_id_write_regsel(DOSBOX_ID_CMD_SET_VGA_CAPTURE_STATE);
    dosbox_id_write_data(0UL); // clear all bits

    return 0;
}

