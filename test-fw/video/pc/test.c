
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

/* WARNING: We trust the C library will still have working STDIO at this point */
void log_atexit(void) {
    LOG("* NORMAL EXIT\n");
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

        LOG("* VIDEO\\PC\\TEST: IBM PC/XT/AT video test group #1\n");
    }

    return 1;
}

void log_cpu_info(void) {
	LOG("CPU: %s or higher\n",cpu_basic_level_to_string(cpu_basic_level));

	if (cpu_v86_active)
		LOG("CPU: Is in virtual 8086 mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE)
		LOG("CPU: CPU is in protected mode\n");
	if (cpu_flags & CPU_FLAG_PROTECTED_MODE_32)
		LOG("CPU: CPU is in 32-bit protected mode\n");
}

void log_dos_info(void) {
	LOG("DOS: version %x.%02u\n",dos_version>>8,dos_version&0xFF);
	LOG("DOS: Method: '%s'\n",dos_version_method);
	LOG("DOS: Flavor: '%s'\n",dos_flavor_str(dos_flavor));
}

void log_windows_info(void) {
    if (windows_mode != WINDOWS_NONE) {
		LOG("WINDOWS: Mode is %s\n",windows_mode_str(windows_mode));
		LOG("WINDOWS: Version %x.%u\n",windows_version>>8,windows_version&0xFF);
		LOG("WINDOWS: Method is %s\n",windows_version_method);
    }
}

uint8_t read_int10_bd_mode(void) {
#if TARGET_MSDOS == 32
    return *((uint8_t*)0x449);
#else
    return *((uint8_t far*)MK_FP(0x40,0x49));
#endif
}

uint16_t int11_info = 0;

int int10_setmode_and_check(uint8_t mode) {
    uint8_t amode;

    int10_setmode(mode);

    if ((amode=read_int10_bd_mode()) != mode) {
        LOG("! INT 10h mode set 0x%02x failed, BIOS is still in mode 0x%02x according to BIOS data area\n",mode,amode);
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
    LOG("INT 11h video mode: ");
    switch ((int11 >> 4) & 0x3) {
        case 0: LOG("EGA or later"); break; 
        case 1: LOG("CGA color 40x25"); break; 
        case 2: LOG("CGA color 80x25"); break; 
        case 3: LOG("MDA monochrome 80x25"); break; 
    }
    LOG("\n");
}

void alphanumeric_test(unsigned int w,unsigned int h) {
    VGA_ALPHA_PTR vmem;
    uint16_t sv;

    int11_info = _bios_equiplist(); /* IBM PC BIOS equipment list INT 11h */
    LOG("INT 11h equipment list: 0x%04x\n",int11_info);
    LOG_INT11_VIDEOMODE(int11_info);

    /* MDA monochrome mode means the text VRAM is at B000:0000
     * Otherwise, the text VRAM is at B800:0000 */
    if (((int11_info >> 4) & 3) == 3)//MDA
        sv = 0xB000u;
    else
        sv = 0xB800u;

    LOG("Therefore, using video RAM segment 0x%04x\n",sv);
#if TARGET_MSDOS == 32
    vmem = (VGA_ALPHA_PTR)(sv << 4u);
    LOG("! Internal ptr: %p\n",vmem);
#else
    vmem = (VGA_ALPHA_PTR)MK_FP(sv,0);
    LOG("! Internal ptr: %Fp\n",vmem);
#endif
}

void test_pause(void) {
	const unsigned long delay = t8254_us2ticks(1000UL);
    unsigned int i;

    for (i=0;i < 3000u;i++) {
		t8254_wait(delay);
        if (kbhit()) {
            unsigned int c = getchex();
            if (c == ' ') break;
        }
    }
}

int main() {
    mkdir("video");
    mkdir("video\\pc");
    if (!log_init()) {
        printf("Unable to open log file %s\n",log_name);
        return 1;
    }

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

	_cli();
	write_8254_system_timer(0); // 18.2
	_sti();

    /* basic pattern tests */
    /* we need the screen */
    log_noecho();

    LOG("* Testing: INT 10h mode 0 40x25 mono text mode\n");
    int10_setmode_and_check(0); // will LOG if mode set failure, any sane IBM PC BIOS will have this mode though
    alphanumeric_test(40,25); // should be 40x25
    test_pause();

    /* set back to mode 3 80x25 text */
    int10_setmode(3);
    log_doecho();
    LOG("* Restoring 80x25 text mode\n");

	if (!probe_vga()) {
		printf("Video probe failed\n");
		return 1;
	}

	return 0;
}

