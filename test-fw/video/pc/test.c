
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

uint8_t read_int10_bd_mode(void) {
#if TARGET_MSDOS == 32
    return *((uint8_t*)0x449);
#else
    return *((uint8_t far*)MK_FP(0x40,0x49));
#endif
}

uint16_t int10_bd_read_cga_crt_io(void) {
#if TARGET_MSDOS == 32
    return *((uint16_t*)0x463);
#else
    return *((uint16_t far*)MK_FP(0x40,0x63));
#endif
}

uint8_t int10_bd_read_cga_mode_byte(void) {
#if TARGET_MSDOS == 32
    return *((uint8_t*)0x465);
#else
    return *((uint8_t far*)MK_FP(0x40,0x65));
#endif
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

void test_pause(unsigned int secs) {
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
        }
    }
}

void alphanumeric_test(unsigned int w,unsigned int h) {
    unsigned int o=0,i,x,y;
    VGA_ALPHA_PTR vmem;
    uint16_t crtbase;
    uint16_t sv;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG(LOG_DEBUG "INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

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
        if (vmem[i] != (0x0F0F ^ i ^ (i << 13))) {
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

	_cli();
	write_8254_system_timer(0); // 18.2
	_sti();

    /* basic pattern tests */
    /* we need the screen */
    log_noecho();

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

    /* set back to mode 3 80x25 text */
    int10_setmode(3);
    log_doecho();
    LOG(LOG_DEBUG "Restoring 80x25 text mode\n");

	return 0;
}

