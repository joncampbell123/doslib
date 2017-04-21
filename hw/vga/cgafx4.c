
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

static inline void __ASM_WAITFORVSYNC(void) {
        __asm {
                ; wait for vsync
                mov         dx,3DAh
wvsync:         in          al,dx
                test        al,8
                jz          wvsync

                ; wait for (vsync | blank) end
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
    /* NTS: In this demo, the row start and height must be a multiple of 8.
     *      Because 40x25 text mode, which has a rowheight of 8, and 640x200 2-color mode, with a rowheight of 2.
     *      We don't want to cause any condition where the 6845 row counter runs off beyond 8 (and up through 31 back around to 0) */
    unsigned short row=8; /* cannot be <= 1, multiple of 8 */
    unsigned short rowheight=8; /* RECOMMENDED!! Multiple of 8 */
    unsigned short rowa=1;
    unsigned short rowad=0;
    unsigned short vtadj = (rowheight / 2) - 1; /* adjust to vtotal because text rows become graphics */
    unsigned short row1,row2,row3;
    unsigned char rc1,rc2,rc3;

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
        row2 = rowheight - 1;           /* number of scanlines to wait for row effect. again, want to change char height on last line */
        rc2 = row2 & 1;                 /* predict what the 6845 row scanline counter will be on the scan line PRIOR to ending our raster effect */
        rc3 = 7 - ((row + rowheight) & 7); /* and then decide the character row height - 1 just after the effect to keep total scanlines sane */
#ifdef ANTI_JUMP_ADJUST
        if (row & 7) rc3 += 8;          /* try to prevent the text below the raster effect from "jumping" when not starting on 8-line boundaries */
#endif
        row3 = rc3 + 1;

        __ASM_SETCHARMAXLINE(7);        /* 8 lines */
        __ASM_WAITFORVSYNC();

        __ASM_SETMODESEL(0x28);         /* 40x25 text */
        __ASM_WAITLINES(waitv1,row1);   /* wait until the scanline BEFORE the raster effect */
        __ASM_SETCHARMAXLINE(rc1);      /* write our prediction to char row height to make sure it matches, causing the 6845 to reset row scanline counter to 0 */
        __ASM_WAITLINE();               /* wait one more scanline */

        __ASM_SETMODESEL(0x1A);         /* 640x200 2-color graphics with colorburst enabled */
        __ASM_SETCHARMAXLINE(1);        /* 2 lines */
        __ASM_WAITLINES(waitv2,row2);   /* rowheight - 1 */
        __ASM_SETCHARMAXLINE(rc2);      /* write our prediction to char row height to make sure it matches, reset row scanline counter to 0 */
        __ASM_WAITLINE();               /* wait one more scanline */

        __ASM_SETMODESEL(0x28);         /* 40x25 text */
        __ASM_SETCHARMAXLINE(rc3);      /* make the text row just enough scanlines to even it out */
        __ASM_WAITLINES(waitv1,row3);   /* wait just that many scanlines */
        __ASM_SETCHARMAXLINE(7);        /* 8 lines */

        __asm {
                pop         dx
                pop         cx
        }

        /* do this AFTER to make sure we use the time between our effect and end of display to keep things stable */
        {
            /* because we're changing row height, we need to adjust vtotal/vdisplay to keep normal timing */
//            unsigned int n_rows = ((0x1F/*total*/ + 1) * 8) + 6/*adjust*/; /* = 262 lines aka one normal NTSC field */
            unsigned int rc = 0x1F + 1 + vtadj;

#ifdef ANTI_JUMP_ADJUST
            /* don't need the hack */
#else
            /* hack, fix this later */
            if ((row & 7) != 0) rc++;
#endif

            /* program the new vtotal/vadjust to make the video timing stable */
            outp(0x3D4,0x04); /* vtotal */
            outp(0x3D5,rc-1);
            outp(0x3D4,0x07); /* vsyncpos */
            outp(0x3D5,rc-1-3);
            outp(0x3D4,0x06); /* vdisplay */
            outp(0x3D5,rc-1-3-3);
        }

#ifdef FAST_MOVEMENT
        rowad = 0;
        row += rowa;

        if ((row+rowheight) >= 180) /* the vtotal/vadjust code takes too long to go beyond this point without glitches */
            rowa = -1;
        else if (row <= 2)
            rowa = 1;
#else
        if (++rowad == 60) {/* assume 60Hz */
            rowad = 0;
            row += rowa;

            if ((row+rowheight) >= 180) /* the vtotal/vadjust code takes too long to go beyond this point without glitches */
                rowa = -1;
            else if (row <= 2)
                rowa = 1;
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

