
#include <stdio.h>
#include <conio.h> /* this is where Open Watcom hides the outp() etc. functions */
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <dos.h>

#include <hw/cpu/cpu.h>
#include <hw/dos/dos.h>
#include <hw/vga/vga.h>
#include <hw/8254/8254.h>
#include <hw/8259/8259.h>
#include <hw/vga/vgatty.h>
#include <hw/dos/doswin.h>

#define __ASM_SETCHARMAXLINE(h)                         \
        __asm {                                         \
            __asm   mov         dx,3D4h                 \
            __asm   mov         al,9h                   \
            __asm   out         dx,al                   \
            __asm   inc         dx                      \
            __asm   mov         al,h                    \
            __asm   out         dx,al                   \
        }

#define __ASM_SETMODESEL(x)                             \
        __asm {                                         \
            __asm   mov         dx,3D8h                 \
            __asm   mov         al,x                    \
            __asm   out         dx,al                   \
        }

static inline void __ASM_WAITFORVSYNCBEGIN(void) {
        __asm {
                ; wait for vsync
                mov         dx,3DAh
wvsync:         in          al,dx
                test        al,8
                jz          wvsync
        }
}

static inline void __ASM_WAITFORVSYNCEND(void) {
        __asm {
                ; wait for (vsync | blank) end
                mov         dx,3DAh
wvsyncend:      in          al,dx
                test        al,9        ; 8 (vsync) | 1 (blank)
                jnz         wvsyncend
        }
}

static inline void __ASM_WAITLINE_INNER(void) {
        /* caller should have loaded DX = 3DAh */
        __asm {
                ; wait for hblank
wvsync:         in          al,dx
                test        al,1        ; 1 blank
                jz          wvsync

                ; wait for hblank end
wvsyncend:      in          al,dx
                test        al,1        ; 1 blank
                jnz         wvsyncend
        }
}

static inline void __ASM_WAITLINE(void) {
        __asm mov dx,3DAh

        __ASM_WAITLINE_INNER();
}

#define __ASM_WAITLINES(name,c)                             \
        __asm {                                             \
            __asm   mov         dx,3DAh                     \
            __asm   mov         cx,c                        \
            __asm p1##name:                                 \
            __asm   in          al,dx                       \
            __asm   test        al,1                        \
            __asm   jz          p1##name                    \
            __asm p2##name:                                 \
            __asm   in          al,dx                       \
            __asm   test        al,1                        \
            __asm   jnz         p2##name                    \
            __asm   loop        p1##name                    \
        }

int main() {
    unsigned short row=8; /* cannot be <= 1, multiple of 8 */
    unsigned short rowa=8;
    unsigned short rowad=0;
    unsigned short row1;
    unsigned char rc1,rn1,rn2;

    printf("WARNING: Make sure you run this on CGA hardware!\n");
    printf("Hit ENTER to continue, any other key to exit\n");

    if (getch() != 13) return 1;

    __asm {
            mov     ax,1
            int     10h
    }

    printf("40x25 mode select raster fx test.\n");
    printf("Hit ESC at any time to stop.\n");

    /* we also need to fill the remainder of RAM with something */
    printf("\n");
    {
        unsigned int x,y;

        for (y=0;y < 16;y++) {
            for (x=0;x < 39;x++) {
                printf("%c",(x+y)|0x20);
            }

            printf("\n");
        }
    }

    /* this effect is more convincing if we can move the hardware cursor higher on the screen */
    outp(0x3D4,0x0E);
    outp(0x3D5,0x00);
    outp(0x3D4,0x0F);
    outp(0x3D5,0x00);

    /* the 640x200 portion isn't going to show anything unless we set port 3D9h to set foreground to white.
     * this will also mean the border will be white for text mode, but, meh */
    outp(0x3D9,0x0F);

    _cli();
    do {
        if (inp(0x64) & 1) { // NTS: This is clever because on IBM PC hardware port 64h is an alias of 60h.
            unsigned char c = inp(0x60); // chances are the scancode has bit 0 set.
            // remember that on AT hardware, port 64h bit 0 is set when a keyboard scancode has come in.
            // on PC/XT hardware, 64h is an alias of port 60h, which returns the latest scancode transmitted
            // by the keyboard. to keep the XT keyboard going, we have to set bit 7 of port 61h to clear
            // and restart the scanning process or else we won't get any more scan codes. setting bit 7
            // on AT hardware usually has no effect. We also have to ack IRQ 1 to make sure keyboard
            // interrupts work when we're done with the program.
            //
            // by reading port 64h and 60h in this way we can break out of the loop reliably when the user
            // hits a key on the keyboard, regardless whether the keyboard controller is AT or PC/XT hardware.

            if (c != 0 && (c & 0x80) == 0)
                break;
        }

        {
            unsigned char x = inp(0x61) & 0x7F;
            outp(0x61,x | 0x80);
            outp(0x61,x);

            outp(0x20,P8259_OCW2_SPECIFIC_EOI | 1); // IRQ1
        }

        /* begin */
        __asm {
                push        cx
                push        dx
        }

        row1 = row - 1;
        rc1 = row1 & 7;                 /* predict what the 6845 row scanline counter will be on the scan line PRIOR to our raster effect */

        rn1 = (row >> 3);               /* predict the row count at the raster effect */
        if (row & 7) rn1++;

        rn2 = (262 - 6 - row) >> 3;     /* number of scanlines to complete the picture */

        __ASM_SETCHARMAXLINE(7);        /* 8 lines */
        __ASM_WAITFORVSYNCBEGIN();
        __ASM_WAITFORVSYNCEND();

        /* setup vtotal to end at the row the 6845 will be at when we force the row to end */
        outp(0x3D4,0x04); /* vtotal (register contents are: character line minus 1) */
        outp(0x3D5,rn1-1);
        outp(0x3D4,0x05); /* vadjust */
        outp(0x3D5,0x00);
        outp(0x3D4,0x06); /* vdisplay (register contents are: number of displayed character rows) */
        outp(0x3D5,0x7F); /* move it out of the way */
                          /* ^ NTS: If we set this to "rn1" (to match our display) the CGA will exhibit this odd
                           *        behavior where at multiples of 8 character rows the screen beyond this point
                           *        will flash the blanking interval in sync with the hardware cursor instead of
                           *        displaying text. Weird. */
        outp(0x3D4,0x07); /* vsyncpos (value minus 1) */
        outp(0x3D5,0x7F); /* move it out of the way */

        __ASM_WAITLINES(waitv1,row1);   /* wait until the scanline BEFORE the raster effect */
        __ASM_SETCHARMAXLINE(rc1);      /* write our prediction to char row height to make sure it matches, causing the 6845 to reset row scanline counter to 0 */
        __ASM_WAITLINE();               /* wait one more scanline */

        __ASM_SETCHARMAXLINE(7);        /* 8 lines */

        /* setup vtotal to complete the display normally */
        outp(0x3D4,0x04); /* vtotal (value minus 1) */
        outp(0x3D5,rn2-1);
        outp(0x3D4,0x05); /* vadjust */
        outp(0x3D5,0x06);
        outp(0x3D4,0x06); /* vdisplay */
        outp(0x3D5,rn2-6);
        outp(0x3D4,0x07); /* vsyncpos (value minus 1) */
        outp(0x3D5,rn2-1-3);

        __asm {
                pop         dx
                pop         cx
        }

#ifdef FAST_MOVEMENT
        rowad = 0;
        row += rowa;

        if (row >= 180) /* the vtotal/vadjust code takes too long to go beyond this point without glitches */
            rowa = -8;
        else if (row <= 9)
            rowa = 8;
#else
        if (++rowad == 60) {/* assume 60Hz */
            rowad = 0;
            row += rowa;

            if (row >= 180) /* the vtotal/vadjust code takes too long to go beyond this point without glitches */
                rowa = -8;
            else if (row <= 9)
                rowa = 8;
        }
#endif
    } while(1);

    {
        unsigned char x = inp(0x61) & 0x7F;
        outp(0x61,x | 0x80);
        outp(0x61,x);

        outp(0x20,P8259_OCW2_SPECIFIC_EOI | 1); // IRQ1
    }

    _sti();
    while (kbhit()) getch();

    __asm {
            mov     ax,3
            int     10h
    }

    return 0;
}

