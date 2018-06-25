
#include <sys/types.h>
#include <sys/stat.h>
#include <direct.h>
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdarg.h>
#include <fcntl.h>
#include <math.h>
#include <bios.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

char tmp[1024];
char log_tmp[1024];
FILE *log_fp = NULL;

unsigned char log_echo = 1;
unsigned char log_atexit_set = 0;

const char log_name[] = "video\\pc\\test.log";

void log_noecho(void) {
    log_echo = 0;
}

void log_doecho(void) {
    log_echo = 1;
}

void log_close(void) {
    if (log_fp != NULL) {
        fclose(log_fp);
        log_fp = NULL;
    }
}

void LOG(const char *fmt,...) {
    va_list va;
    int l;

    va_start(va,fmt);
    l = vsnprintf(log_tmp,sizeof(log_tmp),fmt,va);//NTS:I trust snprintf includes the NUL at the end in the string limit!
    va_end(va);

    if (log_fp != NULL)
        fwrite(log_tmp,l,1,log_fp);
    if (log_echo && stdout != NULL) {
        fwrite(log_tmp,l,1,stdout);
        fflush(stdout);
    }
}

#define LOG_DEBUG   "DEBUG> "
#define LOG_INFO    "INFO> "
#define LOG_WARN    "WARN> "
#define LOG_ERROR   "ERROR> "

/* WARNING: We trust the C library will still have working STDIO at this point */
void log_atexit(void) {
    LOG(LOG_INFO "NORMAL EXIT\n");
    log_close();
}

int log_init(void) {
    if (log_fp == NULL) {
        if ((log_fp=fopen(log_name,"w")) == NULL)
            return 0;

        if (!log_atexit_set) {
            log_atexit_set = 1;
            atexit(log_atexit);
        }
    }

    return 1;
}

void log_cpu_info(void) {
	LOG(LOG_INFO "CPU: %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	if (cpu_v86_active)
		LOG(LOG_INFO "CPU: Is in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE)
		LOG(LOG_INFO "CPU: CPU is in protected mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE_32)
		LOG(LOG_INFO "CPU: CPU is in 32-bit protected mode\n");
}

void log_dos_info(void) {
	LOG(LOG_INFO "DOS: version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	LOG(LOG_INFO "DOS: Method: '%s'\n",dos_version_method);
	LOG(LOG_INFO "DOS: Flavor: '%s'\n",dos_flavor_str(dos_flavor));
}

void log_windows_info(void) {
    if (windows_mode != WINDOWS_NONE) {
		LOG(LOG_INFO "WINDOWS: Mode is %s\n",windows_mode_str(windows_mode));
		LOG(LOG_INFO "WINDOWS: Version %x.%u\n",windows_version>>8,windows_version&0xFF);
		LOG(LOG_INFO "WINDOWS: Method is %s\n",windows_version_method);
    }
}

void log_vga_info(void) {
    LOG(LOG_INFO "According to VGA library, video hardware is compatible with:\n");
    if (vga_state.vga_flags & VGA_IS_MDA)
        LOG(LOG_INFO "- MDA\n");
    if (vga_state.vga_flags & VGA_IS_CGA)
        LOG(LOG_INFO "- CGA\n");
    if (vga_state.vga_flags & VGA_IS_AMSTRAD)
        LOG(LOG_INFO "- AMSTRAD\n");
    if (vga_state.vga_flags & VGA_IS_TANDY)
        LOG(LOG_INFO "- TANDY\n");
    if (vga_state.vga_flags & VGA_IS_MCGA)
        LOG(LOG_INFO "- MCGA\n");
    if (vga_state.vga_flags & VGA_IS_HGC)
        LOG(LOG_INFO "- HGC\n");
    if (vga_state.vga_flags & VGA_IS_EGA)
        LOG(LOG_INFO "- EGA\n");
    if (vga_state.vga_flags & VGA_IS_VGA)
        LOG(LOG_INFO "- VGA\n");
}

uint8_t read_bda8(unsigned int ofs) {
#if TARGET_MSDOS == 32
    return *((uint8_t*)(0x400 + ofs));
#else
    return *((uint8_t far*)MK_FP(0x40,ofs));
#endif
}

uint16_t read_bda16(unsigned int ofs) {
#if TARGET_MSDOS == 32
    return *((uint16_t*)(0x400 + ofs));
#else
    return *((uint16_t far*)MK_FP(0x40,ofs));
#endif
}

uint8_t read_int10_bd_mode(void) {
    return read_bda8(0x49);
}

uint16_t int10_bd_read_cga_crt_io(void) {
    return read_bda16(0x63);
}

uint8_t int10_bd_read_cga_mode_byte(void) {
    return read_bda8(0x65);
}

uint16_t int11_info = 0;

int int10_setmode_and_check(uint8_t mode) {
    uint8_t amode;

    int10_setmode(mode);

    if ((amode=read_int10_bd_mode()) != mode) {
        LOG(LOG_WARN "INT 10h mode set 0x%02x failed, BIOS is still in mode 0x%02x according to BIOS data area\n",mode,amode);
        return 0;
    }

    return 1;
}

unsigned int getchex(void) {
    unsigned int c;

    c = getch();
    if (c == 0) c = getch() << 8;

    return c;
}

void LOG_INT11_VIDEOMODE(uint16_t int11) {
    LOG(LOG_INFO "INT 11h video mode: ");
    switch ((int11 >> 4) & 0x3) {
        case 0: LOG("EGA or later"); break; 
        case 1: LOG("CGA color 40x25"); break; 
        case 2: LOG("CGA color 80x25"); break; 
        case 3: LOG("MDA monochrome 80x25"); break; 
    }
    LOG("\n");
}

const char hexes[16] = "0123456789ABCDEF";

int test_pause_10ths(unsigned int dsecs) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    dsecs *= 100u;
    for (i=0;i < dsecs;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
            else if (c == 'x') {
                while (getch() != 13);
                break;
            }
            else if (c == 27)
                return 0;
        }
    }

    return 1;
}

int test_pause(unsigned int secs) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    secs *= 1000u;
    for (i=0;i < secs;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
            else if (c == 'x') {
                while (getch() != 13);
                break;
            }
            else if (c == 27)
                return 0;
        }
    }

    return 1;
}

#define EGARGB2(r,g,b) \
    (((((b) & 2) >> 1) + (((g) & 2) << 0) + (((r) & 2) << 1)) + \
     ((((b) & 1) << 3) + (((g) & 1) << 4) + (((r) & 1) << 5)))

unsigned char egapalac16[16] = {
    EGARGB2(0,0,0),
    EGARGB2(0,0,2),
    EGARGB2(0,2,0),
    EGARGB2(0,2,2),
    EGARGB2(2,0,0),
    EGARGB2(2,0,2),
    EGARGB2(2,2,0),
    EGARGB2(2,2,2),

    EGARGB2(1,1,1),
    EGARGB2(1,1,3),
    EGARGB2(1,3,1),
    EGARGB2(1,3,3),
    EGARGB2(3,1,1),
    EGARGB2(3,1,3),
    EGARGB2(3,3,1),
    EGARGB2(3,3,3)
};

unsigned char egapalac64[16] = {
    EGARGB2(0,0,0),
    EGARGB2(0,0,2),
    EGARGB2(0,2,0),
    EGARGB2(0,2,2),
    EGARGB2(2,0,0),
    EGARGB2(2,0,2),
    EGARGB2(2,1,0),
    EGARGB2(2,2,2),

    EGARGB2(1,1,1),
    EGARGB2(1,1,3),
    EGARGB2(1,3,1),
    EGARGB2(1,3,3),
    EGARGB2(3,1,1),
    EGARGB2(3,1,3),
    EGARGB2(3,3,1),
    EGARGB2(3,3,3)
};

void ega_test(unsigned int w,unsigned int h) {
    unsigned int o,i,x,y;
    unsigned char attrpal[16];
    volatile VGA_RAM_PTR vmem; // do not optimize this code, EGA/VGA planar operations require it

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    LOG(LOG_DEBUG "EGA 16-color planar test %u x %u\n",w,h);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do EGA!
        LOG(LOG_WARN "EGA 16-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xA000u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xA000u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }

    /* If this is MCGA/VGA then read the EGA palette back out */
    if ((vga_state.vga_flags & (VGA_IS_MCGA|VGA_IS_VGA))) {
        for (i=0;i < 16;i++) attrpal[i] = vga_read_AC(i);
        LOG(LOG_DEBUG "Reading AC palette: ");
        for (i=0;i < 16;i++) LOG("0x%02x ",attrpal[i]);
        LOG("\n");

        /* also log the VGA palette */
        LOG(LOG_DEBUG "Reading VGA palette:\n");
        for (i=0;i < 256;i++) {
            unsigned char r,g,b;

            /* NTS: Remember that the VGA has one port for setting READ index, another for WRITE index.
             *      Don't use one to do the other, weird things will happen. At the very least, you'll
             *      read the color palette index off by 1 */
            outp(0x3C7,i);
            r = inp(0x3C9);
            g = inp(0x3C9);
            b = inp(0x3C9);

            if ((i&7) == 0) LOG(LOG_DEBUG " palette 0x%02x-0x%02x = ",i,i+7);
            LOG("%02xh %02xh %02xh  ",r,g,b);
            if ((i&7) == 7) LOG("\n");
        }
    }

    /* test that the RAM is there, note if it is not.
     * although EGA/VGA bioses generally set the mode
     * with all planes enabled at once on write, program
     * the registers to make sure */
    /* NTS: This test will FAIL for 640x350 4-color mode.
     *      I don't know how to support EGA cards with 64kb */
    vga_write_GC(0x05/*mode register*/,0x00);  // read mode=0  write mode=0
    vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
    for (x=0;x < 4;x++) {
        vga_write_sequencer(0x02/*map mask*/,1 << x);// plane to write
        vga_write_GC(0x04/*read map select*/,x);// plane to read

        for (i=0;i < 0x4000/*16KB*/;i++)
            vmem[i] = 0x0F ^ i ^ (i << 6);

        for (i=0;i < 0x4000/*16KB*/;i++) {
            if (vmem[i] != ((0x0F ^ i ^ (i << 6)) & 0xFF)) {
                LOG(LOG_WARN "VRAM TEST FAILED, data written did not read back at byte offset 0x%x\n",i);
                return;
            }
        }

        for (i=0;i < 0x4000;i++)
            vmem[i] = 0;
    }
    vga_write_sequencer(0x02/*map mask*/,0x0F);// all planes

    /* EGA memory layout:
     * 4 bitplanes, each 16KB, 32KB, or 64KB in size.
     * One bit from each bitplane is combined together
     * to make 4 bits per pixel.
     *
     * The original 64KB EGA had a hacked 640x350 4-color
     * mode which seems to use odd/even mode in which
     * each bitplane is only 16KB, but each bitplane pair
     * combines together into one 32KB, giving only two
     * bitplanes.
     *
     * I don't know how to support the 4-color 640x350 mode. */
    __asm {
        mov     ah,0x02     ; set cursor pos
        mov     bh,0x00     ; page 0
        xor     dx,dx       ; DH=row=0  DL=col=0
        int     10h
    }

    sprintf(tmp,"%ux%u 16-col gr. seg 0x%04x, mod 0x%02x\r\n",w,h,0xA000,read_int10_bd_mode());
    for (i=0;tmp[i] != 0;i++) {
        unsigned char cv = tmp[i];

        __asm {
            mov     ah,0x0E     ; teletype output
            mov     al,cv
            xor     bh,bh
            mov     bl,0x0F     ; foreground color (white)
            int     10h
        }
    }

    /* color band. assume that part of the screen is clear. write mode 0, plane by plane. */
    vga_write_GC(0x03/*Data rotate*/,0x00);    // no rotation, no ROP, data is unmodified
    vga_write_GC(0x05/*mode register*/,0x00);  // read mode=0  write mode=0
    vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
    for (y=19;y <= 19;y++) {
        o = y * (w / 8u);

        vga_write_sequencer(0x02/*map mask*/,0x01);// plane 0
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x02);// plane 1
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x04);// plane 2
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x08);// plane 3
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;
    }
    for (y=20;y < 36;y++) {
        o = y * (w / 8u);

        vga_write_sequencer(0x02/*map mask*/,0x01);// plane 0
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((x / (16u/8u)) & 1) ? 0xFF : 0x00;

        vga_write_sequencer(0x02/*map mask*/,0x02);// plane 1
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((x / (16u/8u)) & 2) ? 0xFF : 0x00;

        vga_write_sequencer(0x02/*map mask*/,0x04);// plane 2
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((x / (16u/8u)) & 4) ? 0xFF : 0x00;

        vga_write_sequencer(0x02/*map mask*/,0x08);// plane 3
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((x / (16u/8u)) & 8) ? 0xFF : 0x00;
    }
    for (y=36;y < 52;y++) {
        o = y * (w / 8u);

        vga_write_sequencer(0x02/*map mask*/,0x01);// plane 0
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((((x          ) / (16u/8u)) & 1) ? (0x55 << (y&1)) : 0x00) |
                                               ((((x + (8u/8u)) / (16u/8u)) & 1) ? (0xAA >> (y&1)) : 0x00);

        vga_write_sequencer(0x02/*map mask*/,0x02);// plane 1
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((((x          ) / (16u/8u)) & 2) ? (0x55 << (y&1)) : 0x00) |
                                               ((((x + (8u/8u)) / (16u/8u)) & 2) ? (0xAA >> (y&1)) : 0x00);

        vga_write_sequencer(0x02/*map mask*/,0x04);// plane 2
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((((x          ) / (16u/8u)) & 4) ? (0x55 << (y&1)) : 0x00) |
                                               ((((x + (8u/8u)) / (16u/8u)) & 4) ? (0xAA >> (y&1)) : 0x00);

        vga_write_sequencer(0x02/*map mask*/,0x08);// plane 3
        for (x=0;x < (256/8u);x++) vmem[o+x] = ((((x          ) / (16u/8u)) & 8) ? (0x55 << (y&1)) : 0x00) |
                                               ((((x + (8u/8u)) / (16u/8u)) & 8) ? (0xAA >> (y&1)) : 0x00);
    }
    for (y=52;y <= 52;y++) {
        o = y * (w / 8u);

        vga_write_sequencer(0x02/*map mask*/,0x01);// plane 0
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x02);// plane 1
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x04);// plane 2
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;

        vga_write_sequencer(0x02/*map mask*/,0x08);// plane 3
        for (x=0;x < (256/8u);x++) vmem[o+x] = 0xFF;
    }
 
    /* color band. write mode 1 to copy the first */
    vga_write_sequencer(0x02/*map mask*/,0x0F);// all planes
    vga_write_GC(0x03/*Data rotate*/,0x00);    // no rotation, no ROP, data is unmodified
    vga_write_GC(0x05/*mode register*/,0x01);  // read mode=0  write mode=1
    vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
    for (y=54;y < (54+1+32+1);y++) {
        o = y * (w / 8u);
        i = o - ((54-19) * (w / 8u));

        /* use VOLATILE to prevent the compiler from optimizing this code! */
        for (x=0;x < (w/8u);x++) {
            unsigned char c = (volatile)vmem[i+x];
            vmem[o+x] = c;
        }
    }

    /* color band. write mode 2 */
    vga_write_sequencer(0x02/*map mask*/,0x0F);// all planes
    vga_write_GC(0x03/*Data rotate*/,0x00);    // no rotation, no ROP, data is unmodified
    vga_write_GC(0x05/*mode register*/,0x02);  // read mode=0  write mode=2
    vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
    for (y=89;y <= 89;y++) {
        o = y * (w / 8u);

        for (x=0;x < (256/8u);x++) vmem[o+x] = 15;
    }
    for (y=90;y < 106;y++) {
        o = y * (w / 8u);

        for (x=0;x < (256/8u);x++) vmem[o+x] = x >> 1u;
    }
    for (y=106;y < 122;y++) {
        o = y * (w / 8u);

        vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
        for (x=0;x < (256/8u);x += 2) vmem[o+x] = x >> 1u;

        vga_write_GC(0x08/*bit mask*/,0x55 << (y&1));
        for (x=1;x < (256/8u);x += 2) vmem[o+x] = x >> 1u;

        vga_write_GC(0x08/*bit mask*/,0xAA >> (y&1));
        for (x=1;x < (256/8u);x += 2) {
            volatile unsigned char c = vmem[o+x];//load latches, DO NOT optimize out!
            vmem[o+x] = (x >> 1u) + 1u;
            (void)c;
        }
    }
    vga_write_GC(0x08/*bit mask*/,0xFF);       // all bits
    for (y=122;y <= 122;y++) {
        o = y * (w / 8u);

        for (x=0;x < (256/8u);x++) vmem[o+x] = 15;
    }
 
    test_pause(3);

    /* now play with the EGA palette */
    assert(EGARGB2(0,0,1) == 0x08);
    assert(EGARGB2(0,0,2) == 0x01);
    assert(EGARGB2(0,0,3) == 0x09);
    assert(EGARGB2(1,0,0) == 0x20);
    assert(EGARGB2(2,0,0) == 0x04);
    assert(EGARGB2(3,0,0) == 0x24);
    if (h >= 350) {
        LOG(LOG_DEBUG "Loading EGA palette appropriate for 350/480-line modes\n");
        for (i=0;i < 16;i++) vga_write_AC(i,egapalac64[i]);
    }
    else {
        LOG(LOG_DEBUG "Loading EGA palette appropriate for 200-line modes\n");
        for (i=0;i < 16;i++) vga_write_AC(i,egapalac16[i]);
    }
    vga_write_AC(VGA_AC_ENABLE|0x1F,0);
 
    test_pause(1);

    {
        unsigned char *pal = (h >= 350) ? egapalac64 : egapalac16;

        for (i=0;i < 16;i++) {
            for (o=0;o < 4;o++) {
                vga_write_AC(i,(o & 1) ? pal[i] : (i >= 8 ? 0x00 : 0x3F));
                vga_write_AC(VGA_AC_ENABLE|0x1F,0);
                if (!test_pause_10ths(2)) {
                    vga_write_AC(i,pal[i]);
                    vga_write_AC(VGA_AC_ENABLE|0x1F,0);
                    i=16;
                    o=8;
                    break;
                }
            }
        }

        /* NTS: Testing so far, shows that EGA 200-line modes are limited to a CGA
         *      palette. This is documented on EGA hardware to be a limitation of the
         *      EGA video connector and monitor when in 200-line modes. Otherwise,
         *      this should cycle through smooth gradients of each RGB combination */
        for (i=0;i < 8;i++) {
            for (x=0;x < 4;x++)
                vga_write_AC(x,EGARGB2((i&4)?(x&3):0,(i&2)?(x&3):0,(i&1)?(x&3):0));

            vga_write_AC(VGA_AC_ENABLE|0x1F,0);
 
            if (!test_pause_10ths(8)) break;
        }

        /* restore palette */
        for (i=0;i < 16;i++) vga_write_AC(i,pal[i]);
    }

    /* If this is MCGA/VGA then play with the VGA palette that the EGA palette is mapped to */
    if ((vga_state.vga_flags & (VGA_IS_MCGA|VGA_IS_VGA))) {
    }
}

void cga4_test(unsigned int w,unsigned int h) {
    unsigned int i,x,y;
    VGA_RAM_PTR vmem;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do CGA!
        LOG(LOG_WARN "CGA 4-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xB800u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xB800u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }

    /* test that the RAM is there, note if it is not */
    for (i=0;i < 0x4000/*16KB*/;i++)
        vmem[i] = 0x0F ^ i ^ (i << 6);

    for (i=0;i < 0x4000/*16KB*/;i++) {
        if (vmem[i] != ((0x0F ^ i ^ (i << 6)) & 0xFF)) {
            LOG(LOG_WARN "VRAM TEST FAILED, data written did not read back at byte offset 0x%x\n",i);
            return;
        }
    }

    for (i=0;i < 0x4000;i++)
        vmem[i] = 0;

    /* CGA memory layout:
     * 16KB RAM region
     *
     * First 8KB has the even lines 0, 2, 4, 6, 8....
     * Second 8KB has the odd lines 1, 3, 5, 7, 9....
     * 4 pixels per byte.
     *
     * Getting the 8x8 fonts is a pain on pre-EGA BIOSes, so just go ahead and use INT 10h to print text */
    __asm {
        mov     ah,0x02     ; set cursor pos
        mov     bh,0x00     ; page 0
        xor     dx,dx       ; DH=row=0  DL=col=0
        int     10h
    }

    sprintf(tmp,"%u x %u text at seg 0x%04x, mode 0x%02x\r\n",w,h,0xB800,read_int10_bd_mode());
    for (i=0;tmp[i] != 0;i++) {
        unsigned char cv = tmp[i];

        __asm {
            mov     ah,0x0E     ; teletype output
            mov     al,cv
            xor     bh,bh
            mov     bl,3        ; foreground color (white)
            int     10h
        }
    }

    for (y=16;y < 80;y++) {
        VGA_RAM_PTR d = vmem + ((y>>1u) * (w>>2u)) + ((y&1u) * 0x2000u);
        for (x=0;x < (256u/4u);x++) {
            unsigned char c1 = x / (64u/4u);
            unsigned char c2 = ((x + (32u/4u)) / (64u/4u)) & 3;

            if (y >= 48 && c1 != c2)
                d[x] = (c2 * ((y&1) ? 0x11u : 0x44u)) + (c1 * ((y&1) ? 0x44u : 0x11u));
            else
                d[x] = c1 * 0x55u;
        }
    }

    for (i=1;i <= 3;i++) {
        for (x=0;x < w;x++) {
            y = (x + ((i - 1) * 10)) % 60;
            if (y >= 30) y = 60 - y;
            y += 100;

            {
                VGA_RAM_PTR d = vmem + ((y>>1u) * (w>>2u)) + ((y&1u) * 0x2000u);

                d[x/4u] &= ~((0xC0u) >> ((x & 3) * 2u));
                d[x/4u] |= (((i&3u) * 0x40u) >> ((x & 3) * 2u));
            }
        }
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ah,0x0B
            mov     bh,0x01
            mov     bl,0x00
            int     10h
        }
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x10);
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ah,0x0B
            mov     bh,0x01
            mov     bl,0x01
            int     10h
        }
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x30);
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ah,0x0B
            mov     bh,0x01
            mov     bl,0x00
            int     10h
        }
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x00);
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ah,0x0B
            mov     bh,0x01
            mov     bl,0x01
            int     10h
        }
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x20);
    }

    test_pause(1);
}

void cga2_test(unsigned int w,unsigned int h) {
    unsigned int i,x,y;
    VGA_RAM_PTR vmem;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    if (((int11_info >> 4) & 3) == 3) // MDA can't do CGA!
        LOG(LOG_WARN "CGA 2-color mode allowed to work despite MDA configuration\n");

#if TARGET_MSDOS == 32
    vmem = (VGA_RAM_PTR)(0xB800u << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_RAM_PTR)MK_FP(0xB800u,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != 0x3D4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual for this video mode\n",
                port);
    }

    /* test that the RAM is there, note if it is not */
    for (i=0;i < 0x4000/*16KB*/;i++)
        vmem[i] = 0x0F ^ i ^ (i << 6);

    for (i=0;i < 0x4000/*16KB*/;i++) {
        if (vmem[i] != ((0x0F ^ i ^ (i << 6)) & 0xFF)) {
            LOG(LOG_WARN "VRAM TEST FAILED, data written did not read back at byte offset 0x%x\n",i);
            return;
        }
    }

    for (i=0;i < 0x4000;i++)
        vmem[i] = 0;

    /* CGA memory layout:
     * 16KB RAM region
     *
     * First 8KB has the even lines 0, 2, 4, 6, 8....
     * Second 8KB has the odd lines 1, 3, 5, 7, 9....
     * 8 pixels per byte.
     *
     * Getting the 8x8 fonts is a pain on pre-EGA BIOSes, so just go ahead and use INT 10h to print text */
    __asm {
        mov     ah,0x02     ; set cursor pos
        mov     bh,0x00     ; page 0
        xor     dx,dx       ; DH=row=0  DL=col=0
        int     10h
    }

    sprintf(tmp,"%u x %u text at seg 0x%04x, mode 0x%02x\r\n",w,h,0xB800,read_int10_bd_mode());
    for (i=0;tmp[i] != 0;i++) {
        unsigned char cv = tmp[i];

        __asm {
            mov     ah,0x0E     ; teletype output
            mov     al,cv
            xor     bh,bh
            mov     bl,3        ; foreground color (white)
            int     10h
        }
    }

    for (y=16;y < 80;y++) {
        VGA_RAM_PTR d = vmem + ((y>>1u) * (w>>3u)) + ((y&1u) * 0x2000u);
        for (x=0;x < (512u/8u);x++) {
            unsigned char c1 = x / (256u/8u);
            unsigned char c2 = ((x + (128u/8u)) / (256u/8u)) & 1;

            if (y >= 48 && c1 != c2)
                d[x] = (c2 * ((y&1) ? 0x55u : 0xAAu)) + (c1 * ((y&1) ? 0xAAu : 0x55u));
            else
                d[x] = c1 * 0xFFu;
        }
    }

    for (i=1;i == 1;i++) {
        for (x=0;x < w;x++) {
            y = (x + ((i - 1) * 10)) % 60;
            if (y >= 30) y = 60 - y;
            y += 100;

            {
                VGA_RAM_PTR d = vmem + ((y>>1u) * (w>>3u)) + ((y&1u) * 0x2000u);

                d[x/8u] &= ~(0x80u >> (x & 7));
                d[x/8u] |= (0x80u >> (x & 7));
            }
        }
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        // NONE
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x07);// dim foreground
    }

    test_pause(1);

    /* second palette */
    /* EGA/VGA: Use INT 10h AH=0Bh BH=01h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        // NONE
    }
    else {
        LOG(LOG_DEBUG "Switching CGA palette, port=0x%03x\n",0x3D9);

        outp(0x3D9,0x0F);// bright foreground
    }

    test_pause(1);
}

void alphanumeric_test(unsigned int w,unsigned int h) {
    unsigned int o=0,i,x,y;
    VGA_ALPHA_PTR vmem;
    uint16_t crtbase;
    uint16_t sv;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    /* check columns */
    LOG(LOG_DEBUG "BIOS data area reports %u columns per row\n",read_bda16(0x4A));
    if (read_bda16(0x4A) != w)
        LOG(LOG_WARN "BIOS data area reports different columns/row than our expected %u\n",w);

    /* check rows.
     * I noticed from the 1984 edition of the IBM hardware reference that
     * byte 0x84 in the BDA didn't exist until the 1986 reference,
     * therefore it's best not to check unless EGA/MCGA or better. */
    LOG(LOG_DEBUG "BIOS data area reports %u rows (may not exist if BIOS is old enough)\n",read_bda8(0x84)+1);
    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA)) != 0) {
        y = read_bda8(0x84);
        if (y != 0 && (y+1) != h)
            LOG(LOG_WARN "BIOS data area reports different row count than our expected %u\n",h);
    }

    /* MDA monochrome mode means the text VRAM is at B000:0000
     * Otherwise, the text VRAM is at B800:0000
     * BUT on EGA/VGA despite this equipment check the segment is B000 */
    if (((int11_info >> 4) & 3) == 3) {//MDA
        crtbase = 0x3B4;
        sv = 0xB000u;
    }
    else {
        crtbase = 0x3D4;
        sv = 0xB800u;
    }

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA)) != 0 && read_int10_bd_mode() == 7) {
        LOG(LOG_DEBUG "But this is EGA/VGA/MCGA and INT 10h mode 7, therefore monochrome mode\n");
        crtbase = 0x3B4;
        sv = 0xB000u;
    }

    /* did we determine the CRT base I/O port correctly?
     * check the BIOS data area.
     * EGA/VGA seems to emulate this correctly. */
    {
        uint16_t port = int10_bd_read_cga_crt_io();
        if (port != crtbase)
            LOG(LOG_WARN "BIOS says CRT I/O port is 0x%x, my guess was 0x%x. This might be a problem.\n",
                port,crtbase);

        if (port != 0x3D4 && port != 0x3B4)
            LOG(LOG_WARN "BIOS CRT I/O port in bios DATA area 0x%x is unusual.\n",
                port);
    }

    LOG(LOG_INFO "Therefore, using video RAM segment 0x%04x\n",sv);
#if TARGET_MSDOS == 32
    vmem = (VGA_ALPHA_PTR)(sv << 4u);
    LOG(LOG_DEBUG "Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_ALPHA_PTR)MK_FP(sv,0);
    LOG(LOG_DEBUG "Internal ptr: %Fp\n",vmem);
#endif

    /* test that the RAM is there, note if it is not */
    for (i=0;i < (w * h);i++)
        vmem[i] = 0x0F0F ^ i ^ (i << 13);

    for (i=0;i < (w * h);i++) {
        if (vmem[i] != ((0x0F0F ^ i ^ (i << 13)) & 0xFFFF)) {
            LOG(LOG_WARN "VRAM TEST FAILED, data written did not read back at byte offset 0x%x\n",i);
            return;
        }
    }

    for (i=0;i < (w * h);i++)
        vmem[i] = 0x0720;

    sprintf(tmp,"%u x %u text at seg 0x%04x, mode 0x%02x",w,h,sv,read_int10_bd_mode());
    for (i=0;tmp[i] != 0;i++)
        vmem[i] = 0x0700 + ((unsigned char)tmp[i]);

    sprintf(tmp,"Character map below");
    for (i=0;tmp[i] != 0;i++)
        vmem[i+w] = 0x0700 + ((unsigned char)tmp[i]);

    /* show characters */
    for (i=0;i < 16;i++) {
        vmem[(i*2)+(w*3)+1] = 0x7000 | hexes[i]; /* top row */
        vmem[(i*2)+(w*3)+1+1] = 0x7020;
        vmem[w*(4+i)] = 0x7000 | hexes[i]; /* left column */
    }
    for (y=0;y < 16;y++) {
        for (x=0;x < 16;x++) {
            o = w*(4+y)+(x*2)+1;
            vmem[o+0] =
            vmem[o+1] = 0x0700 + (y * 16) + x;
        }
    }

    test_pause(3);

    sprintf(tmp,"Attribute map below");
    for (i=0;tmp[i] != 0;i++)
        vmem[i+w] = 0x0700 + ((unsigned char)tmp[i]);

    /* show characters */
    for (y=0;y < 16;y++) {
        for (x=0;x < 16;x++) {
            o = w*(4+y)+(x*2)+1;
            vmem[o+0] =
            vmem[o+1] = (((y * 16) + x) << 8) + 'A';
        }
    }

    test_pause(1);

    /* turn on blinking */
    /* EGA/VGA: Use INT 10h AX=1003h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    /* NTS: DOSBox and DOSBox-X appear to have a bug in INT 10h where if we switch off blinking,
     *      INT 10h won't turn it back on */
    sprintf(tmp,"Attribute map below with blinking");
    for (i=0;tmp[i] != 0;i++)
        vmem[i+w] = 0x0700 + ((unsigned char)tmp[i]);

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ax,0x1003
            mov     bl,0x01
            int     10h
        }
    }
    else {
        uint8_t mb = int10_bd_read_cga_mode_byte();

        LOG(LOG_DEBUG "Switching on blink attribute CGA/MDA method, port=0x%03x, BIOS mode byte was 0x%02x\n",
            crtbase+4u,mb);

        outp(crtbase+4u,mb | 0x20);/*turn on bit 5*/
    }

    test_pause(3);

    /* turn off blinking */
    /* EGA/VGA: Use INT 10h AX=1003h
     * CGA/MDA: Read the mode select register value from 40:65, modify, and write to CGA hardware */
    sprintf(tmp,"Attribute map below w/o blinking ");
    for (i=0;tmp[i] != 0;i++)
        vmem[i+w] = 0x0700 + ((unsigned char)tmp[i]);

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        __asm {
            mov     ax,0x1003
            mov     bl,0x00
            int     10h
        }
    }
    else {
        uint8_t mb = int10_bd_read_cga_mode_byte();

        LOG(LOG_DEBUG "Switching off blink attribute CGA/MDA method, port=0x%03x, BIOS mode byte was 0x%02x\n",
            crtbase+4u,mb);

        outp(crtbase+4u,mb & 0xDF);/*turn off bit 5*/
    }

    test_pause(3);
}

int main() {
    mkdir("video");
    mkdir("video\\pc");
    if (!log_init()) {
        printf("Unable to open log file %s\n",log_name);
        return 1;
    }

    LOG(LOG_INFO "VIDEO\\PC\\TEST: IBM PC/XT/AT video test group #1\n");

    /* Expected environment: Pure DOS mode (not Windows or OS/2).
     *                       User may run us under EMM386.EXE or not.
     *                       CPU is expected to be 8086 or higher. */
    cpu_probe();
    log_cpu_info();

	probe_dos();
    log_dos_info();

	detect_windows();
    log_windows_info();

    if (windows_mode != WINDOWS_NONE) {
        printf("This test requires pure DOS mode. Do not run under Windows or OS/2.\n");
        return 1;
    }

	if (!probe_8254()) {
		printf("8254 not found (I need this for time-sensitive portions of the driver)\n");
		return 1;
	}

	if (!probe_vga()) {
		printf("Video probe failed\n");
		return 1;
	}

    log_vga_info();

    // additional debug info
    {
        uint16_t bv=0,cv=0;

        __asm {
            mov     ah,0x12             ; Alternate function select - Get EGA INFO
            mov     bx,0xFF10
            int     10h
            mov     bv,bx
            mov     cv,cx
        }

        LOG(LOG_DEBUG "INT 10h AH=0x12 BL=0x10 Get EGA INFO results:\n"
            LOG_DEBUG "- BH(video state)=0x%02x\n"
            LOG_DEBUG "- BL(installed memory in 64KB)=0x%02x\n"
            LOG_DEBUG "- CH(feature connector bits)=0x%02x\n"
            LOG_DEBUG "- CL(switch settings)=0x%02x\n",
            bv>>8u,bv&0xFFu,
            cv>>8u,cv&0xFFu);

        __asm {
            mov     ax,0x1A00
            xor     bx,bx
            int     10h
            mov     bv,bx
            mov     cv,cx
        }

        LOG(LOG_DEBUG "INT 10h AX=0x1A00 Get Display Combination Code:\n"
            LOG_DEBUG "- BL(active display code)=0x%02x\n"
            LOG_DEBUG "- BH(alternate display code)=0x%02x\n",
            bv&0xFFu,bv>>8u);
    }

	_cli();
	write_8254_system_timer(0); // 18.2
	_sti();

    /* basic pattern tests */
    /* we need the screen */
    log_noecho();

#if 0
    LOG(LOG_INFO "Testing: INT 10h mode 0 40x25 mono text mode\n");
    if (int10_setmode_and_check(0))// will LOG if mode set failure
        alphanumeric_test(40,25); // should be 40x25

    LOG(LOG_INFO "Testing: INT 10h mode 1 40x25 color text mode\n");
    if (int10_setmode_and_check(1))// will LOG if mode set failure
        alphanumeric_test(40,25); // should be 40x25

    LOG(LOG_INFO "Testing: INT 10h mode 2 80x25 mono text mode\n");
    if (int10_setmode_and_check(2))// will LOG if mode set failure
        alphanumeric_test(80,25); // should be 40x25

    LOG(LOG_INFO "Testing: INT 10h mode 3 80x25 color text mode\n");
    if (int10_setmode_and_check(3))// will LOG if mode set failure
        alphanumeric_test(80,25); // should be 40x25

    LOG(LOG_INFO "Testing: INT 10h mode 7 80x25 mono text mode\n");
    if (int10_setmode_and_check(7))// will LOG if mode set failure
        alphanumeric_test(80,25); // should be 40x25

    LOG(LOG_INFO "Testing: INT 10h mode 4 320x200 CGA color 4-color graphics mode\n");
    if (int10_setmode_and_check(4))// will LOG if mode set failure
        cga4_test(320,200);

    LOG(LOG_INFO "Testing: INT 10h mode 5 320x200 CGA mono 4-color graphics mode\n");
    if (int10_setmode_and_check(5))// will LOG if mode set failure
        cga4_test(320,200);

    LOG(LOG_INFO "Testing: INT 10h mode 6 640x200 CGA mono 2-color graphics mode\n");
    if (int10_setmode_and_check(6))// will LOG if mode set failure
        cga2_test(640,200);
#endif

    if ((vga_state.vga_flags & (VGA_IS_EGA|VGA_IS_MCGA|VGA_IS_VGA))) {
        LOG(LOG_INFO "Testing: INT 10h mode 13 320x200 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(13))// will LOG if mode set failure
            ega_test(320,200);

        LOG(LOG_INFO "Testing: INT 10h mode 14 640x200 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(14))// will LOG if mode set failure
            ega_test(640,200);

        LOG(LOG_INFO "Testing: INT 10h mode 16 640x350 EGA 16-color graphics mode\n");
        if (int10_setmode_and_check(16))// will LOG if mode set failure
            ega_test(640,350);

        LOG(LOG_INFO "Testing: INT 10h mode 18 640x480 VGA 16-color graphics mode\n");
        if (int10_setmode_and_check(18))// will LOG if mode set failure
            ega_test(640,480);
    }

    /* set back to mode 3 80x25 text */
    int10_setmode(3);
    log_doecho();
    LOG(LOG_DEBUG "Restoring 80x25 text mode\n");

	return 0;
}

