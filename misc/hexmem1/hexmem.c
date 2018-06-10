/* hexmem memory viewer/dumper
 * (C) 2008-2011 Jonathan Campbell
 *
 * Demonstrates linking to various libraries in the hardware section, and
 * using various methods to dump conventional memory (and if the setup is
 * right, extended memory as well).
 */

/* BUGS:
 *     16-bit real mode "Compact" memory model:
 *          Program crashes instead of exiting to DOS, when run under VirtualBox
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <conio.h>
#include <bios.h>
#include <dos.h>
#include <i86.h>    /* for MK_FP */

#include <hw/dos/dos.h>
#include <hw/dos/emm.h>
#if !defined(TARGET_PC98)
#include <hw/8042/8042.h>
#endif
#include <hw/dos/biosext.h>
#include <hw/dos/himemsys.h>
#include <hw/flatreal/flatreal.h>
#include <hw/llmem/llmem.h>
#if !defined(TARGET_PC98)
#include <hw/vga/vga.h>
#include <hw/vga/vgatty.h>
#endif
#include <hw/dos/doswin.h>

#if !defined(TARGET_PC98)
#define SCREEN_WIDTH  vga_state.vga_width
#define SCREEN_HEIGHT vga_state.vga_height
#else
static unsigned int SCREEN_WIDTH = 80;
static unsigned int SCREEN_HEIGHT = 25;
#endif

#if TARGET_MSDOS == 16
static unsigned long segoff2phys(void far *p) {
    unsigned int seg = FP_SEG(p),off = FP_OFF(p);
    return (((unsigned long)seg << 4UL) + ((unsigned long)off));
}
#endif

static const char *hexen = "0123456789ABCDEF";
static unsigned char datatmp[16*50]; /* max 16 segments on 50-row VGA display */
#if TARGET_MSDOS == 16
static unsigned char test_biosext[32];
#endif

static unsigned char llmem = 0;
#if TARGET_MSDOS == 32
static unsigned char ptrcast = 0;
#endif
#if TARGET_MSDOS == 16
static unsigned char flatreal = 0,flatreal_no = 0;
static unsigned char biosextmem = 0,biosextmem_no = 0;
#endif

static void help() {
    fprintf(stderr,"hexmem [options]\n");
#if TARGET_MSDOS == 32
#else
    fprintf(stderr,"    /nf     Don't use Flat Real Mode to read extended memory\n");
    fprintf(stderr,"    /nb     Don't use BIOS INT 15H to read extended memory\n");
#endif
}

static int parse_args(int argc,char **argv) {
    int i;

    for (i=1;i < argc;) {
        char *a = argv[i++];

        /* allow Linux style -switches or DOS style /switches */
        if (*a == '-' || *a == '/') {
            do { a++; } while (*a == '-');

            if (!strcmp(a,"nf")) {
#if TARGET_MSDOS == 16
                flatreal_no = 1;
#endif
            }
            else if (!strcmp(a,"nb")) {
#if TARGET_MSDOS == 16
                biosextmem_no = 1;
#endif
            }
            else {
                help();
                return 1;
            }
        }
        else {
            /* ignore */
        }
    }

    return 0;
}

static void copy_data_to(unsigned char *dst,uint64_t src,size_t para) {
    if (llmem && (src >= (1ULL << 32ULL) || (src+(para*16ULL)) > (1ULL << 32ULL))) {
        int cpy;

        para *= 16UL;
        while (para > 0) {
            cpy = llmemcpy(llmem_ptr2ofs(dst),src,para);
            if (cpy <= 0) break;
            assert(cpy <= (int)para);
            para -= cpy;
            src += cpy;
            dst += cpy;
        }

        if (para != 0) {
#if TARGET_MSDOS == 32
            memset(dst,0xFF,para);
#else
            _fmemset(dst,0xFF,para);
#endif
        }
    }
#if TARGET_MSDOS == 32
    else if (ptrcast || (src < 0x100000ULL && (src+(para*16ULL)) <= 0x100000ULL)) { /* if no paging, or below 1MB boundary and we know DOS memory is mapped 1:1 */
        unsigned char *srcp = (unsigned char*)((size_t)src);
        memcpy(dst,srcp,para*16);
    }
    else {
        /* then what? */
        memset(dst,0xFF,para*16);
    }
#else /* TARGET_MSDOS == 16 */
    else if (src < 0x100000ULL && (src+(para*16ULL)) <= 0x100000ULL) { /* if below the 1MB mark use normal methods */
        while (para-- != 0UL) {
            _fmemcpy(dst,MK_FP(src>>4,0),16);
            dst += 16;
            src += 16;
        }
    }
    else if (flatreal) {
        _cli();

        /* FIXME: Apparently we have to make sure some part of DOS or BIOS did not reset the segment registers on us */
        if (flatrealmode_test()) {
            if (!flatrealmode_setup(FLATREALMODE_4GB)) {
                fprintf(stderr,"Host OS reset processor and I was not able to restart flat real mode\n");
                _sti();
                abort();
            }
        }

        para *= 16/2;
        while (para-- != 0UL) {
            *((uint16_t*)dst) = flatrealmode_readw(src);
            src += 2;
            dst += 2;
        }
        _sti();
    }
    else if (biosextmem) {
        bios_extcopy(segoff2phys(dst),(uint32_t)src,16UL*para);
    }
    /* TODO: DPMI linear memcpy */
    else {
        /* now what? */
        memset(dst,0xFF,para*16);
    }
#endif
}

int main(int argc,char **argv) {
    int die=0,drawmem=1,x,y,key,i;
    unsigned long repeat=0;
    uint64_t segment=0;

    if (parse_args(argc,argv))
        return 1;

    printf("DOS "); fflush(stdout);
    probe_dos();
    printf("WIN "); fflush(stdout);
    detect_windows();
    printf("CPU "); fflush(stdout);
    cpu_probe();
    printf("DPMI "); fflush(stdout);
    probe_dpmi();
    printf("HIMEM "); fflush(stdout);
    probe_himem_sys();
    printf("EMM "); fflush(stdout);
    probe_emm();
#if TARGET_MSDOS == 32
    printf("LTP "); fflush(stdout);
    dos_ltp_probe();
#endif
    printf("LLMEM "); fflush(stdout);
    llmem = llmem_init();
    printf("[OK]\n");

#if !defined(TARGET_PC98)
    if (!probe_vga())
        return 1;
#endif

#if !defined(TARGET_PC98)
    if (SCREEN_HEIGHT > 50)
        SCREEN_HEIGHT = 50;
#endif

    if (windows_mode != WINDOWS_NONE) {
#if TARGET_MSDOS == 32
        /* well... if we're under Windows we might as well let the user browse through
         * whatever Windows choses to let us use */
        /* FIXME: As code running within NTVDM.EXE poking around where we don't belong only causes NTVDM.EXE to segfault.
         *        Let's not do this until we have a page fault handler installed. */
        /* FIXME: Windows 9x/ME appears to allow reading up to 64KB past 1MB but also page faults our VM */
#else
        /* as a 16-bit app we could abuse DPMI to poke above the 1MB boundary. it's
         * still whatever Windows choses to let us see though */
        /* FIXME: Windows 9x/ME appears to allow reading up to 64KB past 1MB but also page faults our VM */
#endif

        printf("Warning: Memory shown is what Windows choses to make available.\n");
        printf("         It might not reflect actual system memory.\n");
    }

/* question: can we address the entire range of 4GB?
 *     Yes: If the CPU is a 386 or higher and
 *             flat real mode is available, or
 *             we're a 32-bit DOS program and the memory manager is NOT using paging and
 *             we can switch on the A20 gate
 *     Yes: If we're a 286 and HIMEM.SYS or BIOS extended memcpy is available
 *      No: We're running under a Windows DOS Box */
    if (windows_mode == WINDOWS_NONE) {
        if (cpu_basic_level >= 3) { /* 386 or higher */
#if TARGET_MSDOS == 32
            if (!dos_ltp_info.paging) /* if the DOS extender or memory manger is not using paging, then we can just typecast pointers! */
                ptrcast = 1;

            /* TODO: If we can't can we use DPMI to map physical memory? */
#else
            /* Ideal method: Flat real mode aka "Unreal mode" */
            if (!flatreal_no)
                flatreal = flatrealmode_setup(FLATREALMODE_4GB);
#endif
        }
        if (cpu_basic_level >= 2) { /* 286, or 386 type methods failed */
#if TARGET_MSDOS == 16
            if (!biosextmem_no) {
                _cli();
                strcpy(test_biosext+16,"ABCDEFGH");
                /* OK then, does the BIOS provide extended memcpy? */
                memset(test_biosext,0,sizeof(test_biosext));
#if !defined(TARGET_PC98)
                /* direct the BIOS to memcpy() from the realmode vector table */
                bios_extcopy(segoff2phys(test_biosext),segoff2phys(test_biosext+16),8);
#else
                // TODO: PC-98 INT 1Fh...
#endif
                /* did it work? */
                if (_fmemcmp((void far*)test_biosext,(void far*)(test_biosext+16),8) == 0) {
                    /* are the remaining bytes zero? or did the BIOS copy too much? */
                    if (!memcmp(test_biosext+8,"\0\0\0\0\0\0\0\0",8)) {
                        biosextmem = 1;
                    }
                }
                _sti();
            }
#endif
        }
    }

    if (himem_sys_present) {
        /* we need HIMEM.SYS to enable the A20 gate, if not already done */
        himem_sys_global_a20(1);
    }
    else {
#if !defined(TARGET_PC98)
        unsigned char keyb,bb;

        /* try to enable A20 via port 0x92, or the keyboard controller */
        _cli();
        if ((keyb=k8042_probe()) != 0) {
            k8042_drain_buffer();
            k8042_disable_keyboard();

            k8042_write_command(0xD0); /* read input */
            k8042_read_output_wait();
            bb = k8042_read_output();

            k8042_write_command(0xD1);
            k8042_write_data(bb|2|1);

            k8042_drain_buffer();
            k8042_enable_keyboard();
            k8042_drain_buffer();
        }

        /* also mess with port 0x92 to turn on A20 */
        /* see also: http://www.win.tue.nl/~aeb/linux/kbd/A20.html */
        bb = inp(0x92);
        if (!(bb & 2)) outp(0x92,(bb|2)|1);
#else
        outp(0xF6,0x02); // unmask A20 (PC-98)
#endif

        _sti();
    }

    printf("ptrcast=%d flatreal=%d biosext=%d\n",
#if TARGET_MSDOS == 32
        ptrcast,
#else
        0,
#endif

#if TARGET_MSDOS == 16
        flatreal,
        biosextmem);
#else
        0,
        0);
#endif

    printf("LLMEM: %s (%s) limit=0x%llX\n",llmem?"yes":"no",llmem_reason,(unsigned long long)llmem_phys_limit);
    printf("Ready\n");
    getch();

#if defined(TARGET_PC98)
    printf("\x1B[m"); /* normal attributes */
    printf("\x1B[J"); /* clear screen */
    printf("\x1B[1>h"); /* hide function row */
    printf("\x1B[5>h"); /* hide cursor (so we can directly control it) */
    fflush(stdout);
#endif

    while (!die) {
        if (drawmem) {
            unsigned char *rdptr = datatmp;
            copy_data_to(rdptr,segment,SCREEN_HEIGHT);
            for (y=0;y < SCREEN_HEIGHT;y++) {
#if !defined(TARGET_PC98)
                VGA_ALPHA_PTR dst = vga_state.vga_alpha_ram + (y * SCREEN_WIDTH);
                for (x=0;x < 16;x++) {
                    dst[x] =
                        (unsigned short)hexen[((segment+(y*16ULL))>>(60ULL-(x*4ULL)))&0xFULL]|
                        (unsigned short)0x0E00;
                }
                dst[x++] = (unsigned short)' ' | (unsigned short)0x0E00;
                for (i=0;i < 16;i++) {
                    unsigned char b = *rdptr++;
                    dst[x++] = (unsigned short)hexen[b>>4] | (unsigned short)0x0700;
                    dst[x++] = (unsigned short)hexen[b&0xF] | (unsigned short)0x0700;
                    if ((i&1) == 1) dst[x++] = ' ';
                }
                rdptr -= 16;
                dst[x++] = (unsigned short)' ' | (unsigned short)0x0700;
                for (i=0;i < 16;i++) {
                    unsigned char b = *rdptr++;
                    dst[x++] = (unsigned short)b | (unsigned short)0x0D00;
                }
                while (x < SCREEN_WIDTH)
                    dst[x++] = (unsigned short)' ' | (unsigned short)0x0700;
#else
# if TARGET_MSDOS == 16
                uint16_t far *tram = MK_FP(0xA000,0x0000);
                uint16_t far *cram = MK_FP(0xA200,0x0000);
# else
                uint16_t *tram = (uint16_t*)0xA0000;
                uint16_t *cram = (uint16_t*)0xA2000;
# endif
                tram += (y * SCREEN_WIDTH);
                cram += (y * SCREEN_WIDTH);
                for (x=0;x < 16;x++) {
                    tram[x] =
                        (unsigned short)hexen[((segment+(y*16ULL))>>(60ULL-(x*4ULL)))&0xFULL];
                    cram[x] =
                        (unsigned short)0xC1;
                }
                tram[x] = ' ';
                cram[x] = 0xC1;
                x++;
                for (i=0;i < 16;i++) {
                    unsigned char b = *rdptr++;

                    tram[x] = (unsigned short)hexen[b>>4];
                    cram[x] = 0xE1;
                    x++;
                    tram[x] = (unsigned short)hexen[b&0xF];
                    cram[x] = 0xE1;
                    x++;

                    if ((i&1) == 1) {
                        tram[x] = ' ';
                        cram[x] = 0xE1;
                        x++;
                    }
                }
                rdptr -= 16;
                tram[x] = (unsigned short)' ';
                cram[x] = 0xE1;
                x++;
                for (i=0;i < 16;i++) {
                    unsigned char b = *rdptr++;
                    tram[x] = (unsigned short)b;
                    cram[x] = 0x61;
                    x++;
                }
                while (x < SCREEN_WIDTH) {
                    tram[x] = (unsigned short)' ';
                    cram[x] = 0xE1;
                    x++;
                }
#endif
            }
            drawmem=0;
        }

#if !defined(TARGET_PC98)
        key = _bios_keybrd(_KEYBRD_READ);
#else
        {
            unsigned short x=0;

            /* use INT 18h AH=00 */
            __asm {
                xor     ax,ax
                int     18h
                mov     x,ax
            }

            key = x;

            /* translate PC-98 to IBM for this codebase */
            if ((key >> 8) == 0x3A)//uparrow
                key = 0x4800;
            else if ((key >> 8) == 0x3D)//downarrow
                key = 0x5000;
            else if ((key >> 8) == 0x36)//rollup
                key = 0x4900;
            else if ((key >> 8) == 0x37)//rolldown
                key = 0x5100;
        }
#endif

        if ((key&0xFF) == 27) { /* ESC */
            die = 1;
            break;
        }
        else if ((key&0xFF) == 'g') {
            unsigned char input[17];
            int i=0,c;

            drawmem=1;

#if !defined(TARGET_PC98)
            vga_write_color(0x0E);
            vga_clear();
            vga_moveto(0,0);
            vga_write("Jump to: ");
            vga_write_sync();

            while (1) {
                c = getch();
                if (c == 13 || c == 27) break;
                else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) {
                    if (i < 16) {
                        input[i++] = c;
                        vga_writec(c);
                        vga_write_sync();
                    }
                    input[i] = 0;
                }
                else if (c == 8) {
                    if (i > 0) {
                        input[--i] = 0;
                        vga_moveto(vga_state.vga_pos_x-1,vga_state.vga_pos_y);
                        vga_writec(' ');
                        vga_moveto(vga_state.vga_pos_x-1,vga_state.vga_pos_y);
                        vga_write_sync();
                    }
                }
            }
#else
            // TODO
#endif

            if (c == 13) {
                segment = (uint64_t)strtoull(input,NULL,16);
            }
        }
        else if (isdigit((int)(key&0xFF))) { /* vi-style repeat count */
            repeat = (repeat * 10) + ((int)((key&0xFF)-'0'));
        }
        else if ((key&0xFF) == 0) {
            if (repeat == 0) repeat = 1;

            key >>= 8;
            if (key == 0x48) {
                segment -= repeat * 16ULL;
                drawmem=1;
            }
            else if (key == 0x50) {
                segment += repeat * 16ULL;
                drawmem=1;
            }
            else if (key == 0x49) {
                segment -= (unsigned)SCREEN_HEIGHT * repeat * 16ULL;
                drawmem=1;
            }
            else if (key == 0x51) {
                segment += (unsigned)SCREEN_HEIGHT * repeat * 16ULL;
                drawmem=1;
            }

            repeat=0;
        }
    }

#if defined(TARGET_PC98)
    printf("\n");
    printf("\x1B[1>l"); /* show function row */
    printf("\x1B[5>l"); /* show cursor */
    printf("\x1B[24;1H"); /* show cursor */
    fflush(stdout);
#endif

#if TARGET_MSDOS == 16
    flatrealmode_setup(FLATREALMODE_64KB);
#endif
    llmemcpy_free();
    return 0;
}

