
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

int main() {
    /* NTS: In this demo, the row start and height must be a multiple of 8.
     *      Because 40x25 text mode, which has a rowheight of 8, and 640x200 2-color mode, with a rowheight of 2.
     *      We don't want to cause any condition where the 6845 row counter runs off beyond 8 (and up through 31 back around to 0) */
    unsigned short row=8 + 1; /* cannot be zero, multiple of 8 + 1 */
    unsigned short rowheight=8; /* RECOMMENDED!! Multiple of 8 */
    unsigned short vtadj = (rowheight / 2) - 1; /* adjust to vtotal because text rows become graphics */

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

            if (c == 0x39/*SPACE*/) {
                if ((row+rowheight) < 190) row += 8;
            }
            else if (c == 0x0F/*TAB*/) {
                if (row > 8) row -= 8;
            }
            else if (c != 0 && (c & 0x80) == 0)
                break;
        }

        {
            unsigned char x = inp(0x61) & 0x7F;
            outp(0x61,x | 0x80);
            outp(0x61,x);

            outp(0x20,P8259_OCW2_SPECIFIC_EOI | 1); // IRQ1
        }

        {
            /* because we're changing row height, we need to adjust vtotal/vdisplay to keep normal timing */
//            unsigned int n_rows = ((0x1F/*total*/ + 1) * 8) + 6/*adjust*/; /* = 262 lines aka one normal NTSC field */
            unsigned int rc = 0x1F + 1 + vtadj;

            /* program the new vtotal/vadjust to make the video timing stable */
            outp(0x3D4,0x04); /* vtotal */
            outp(0x3D5,rc-1);
            outp(0x3D4,0x07); /* vsyncpos */
            outp(0x3D5,rc-1-3);
            outp(0x3D4,0x06); /* vdisplay */
            outp(0x3D5,rc-1-3-3);
        }

        __asm {
                push        cx
                push        dx

                ; set char height normal
                mov         dx,3D4h
                mov         al,9h
                out         dx,al
                inc         dx
                mov         al,7        ; 8 lines
                out         dx,al

                ; wait for vsync
                mov         dx,3DAh
wvsync:         in          al,dx
                test        al,8
                jz          wvsync

                ; wait for vsync end
wvsyncend:      in          al,dx
                test        al,8
                jnz         wvsyncend

                ; okay, change mode
                mov         dx,3D8h
                mov         al,28h
                out         dx,al

                ; count scanlines
                mov         dx,3DAh
                mov         cx,row
countup1:       in          al,dx       ; wait for hsync
                test        al,1
                jz          countup1

countup2:       in          al,dx       ; wait for hsync end
                test        al,1        
                jnz         countup2
                loop        countup1    ; count rows

                ; okay, change mode to 640x200 but with colorburst on (bit 2 = 0 therefore 0x1E & (~4))
                mov         dx,3D8h
                mov         al,1Ah
                out         dx,al

                ; set char height normal
                mov         dx,3D4h
                mov         al,9h
                out         dx,al
                inc         dx
                mov         al,1        ; 2 lines
                out         dx,al

                ; count scanlines again
                mov         dx,3DAh
                mov         cx,rowheight
countup3:       in          al,dx       ; wait for hsync
                test        al,1
                jz          countup3

countup4:       in          al,dx       ; wait for hsync end
                test        al,1        
                jnz         countup4
                loop        countup3    ; count rows

                ; okay, change mode back
                mov         dx,3D8h
                mov         al,28h
                out         dx,al

                ; set char height normal
                mov         dx,3D4h
                mov         al,9h
                out         dx,al
                inc         dx
                mov         al,7        ; 8 lines
                out         dx,al

                pop         dx
                pop         cx
        }
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

