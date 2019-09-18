
#include <i86.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <hw/vga2/vga2.h>

/* TODO: This API must be replaced.
 *
 *       Replace it with a general "init from video mode" that reads from the BIOS.
 *       That way one function call takes care of video pointer, number of rows and
 *       columns (from BIOS).
 *
 * TODO: Add variables for width and height, byte values. The API will deliberately
 *       use the width as the stride (bytes per line) and will not maintain a
 *       separate variable for stride. It is not this code's responsibility to
 *       provide for rendering text at an offset within the bounds of a display.
 *
 * TODO: Separate variables and source code file if the program wants to manage
 *       cursor position as well. Will have internal variables for X and Y and
 *       functions to copy to/from the BIOS data area. */

vga2_alpha_base_t vga2_alpha_base = {
#ifndef TARGET_PC98 /* PC-98 segment is always 0xA000, IBM PC can be 0xB800 or 0xB800 and even 0xA000 is possible. */
 #if TARGET_MSDOS == 32
    .memptr = NULL,
    .memsz = 0,
 #else
    .segptr = 0,
    .segsz = 0,
 #endif
#endif
    .width = 80,                /* reasonable defaults, for both IBM PC and NEC PC-98 */
    .height = 25
};

/* PC-98 notes:
 *
 * - There is a 40 column mode, but it's really just 80-column mode with every other character
 *   cell skipped and the character bitmap sent to raster with each pixel doubled. Most PC-98
 *   games will write the character twice to video memory to make sure it displays properly.
 *   The WORDs per row do not change between 40-column and 80-column mode.
 *
 *   In fact it's rare for the text layer to change columns per line at all, unless the game
 *   is willing to reprogram it (which does not happen often).
 *
 *   If you are coding for PC-98 and will be doing that, update the width yourself. */

/* IBM PC notes:
 *
 * - EGA/VGA/SVGA hardware uses the Offset register (CRTC reg 13h) to determine character cells
 *   per scanline. The offset register is character cells per scanline divided by 2 because
 *   the CRTC in text mode is set to WORD mode. Because of this, odd bytes per scanline are
 *   impossible in hardware.
 *
 * - MDA, CGA and PCjr hardware do not have an Offset register. The bytes per scanline
 *   are determined by the Horizontal Display End register, which seems to permit (in documentation
 *   anyway) an odd number of character cells.
 *
 * - MCGA is the strange case. MCGA is supposed to be a superset of CGA but some of the CRTC
 *   registers are fake or nonsensical. Register dumps of MCGA hardware for each mode show
 *   that, for whatever reason, all modes have Horizontal display parameters set as if
 *   40x25 or 320x200 mode including the 80x25 text modes. This means that the Horizontal
 *   Display End value holds 39 (40-1) even when 80x25 mode is active. Since MCGA lacks the
 *   Offset register (as does the CGA) this means that only an even number of character cells
 *   are possible on the MCGA. (TODO: Confirm this!)
 *
 *   Note that on the MCGA there is a hardware bit to select whether horizontal timing is
 *   controlled by CRTC registers 00h-03h, or whether they're made up in MCGA hardware and
 *   CRTC 00h-03h are ignored. Values programmed into the Mode Control Register (CRTC reg
 *   10h) have bit 3 set by the BIOS for all video modes, which means that the MCGA hardware
 *   makes up the horizontal timings. While this keeps video stable no matter what tricks
 *   are tried by DOS games, it does bring up the question whether it locks horizontal
 *   timing only or whether it also prevents reprogramming the active display area
 *   (and therefore stride) as well. (TODO: Confirm this!) */

/* this is a function pointer so that specialty code, such as PCjr support, can
 * provide it's own version to point at wherever the video memory is. */
void                            (*vga2_update_alpha_modeinfo)(void) = vga2_update_alpha_modeinfo_default;

void vga2_update_alpha_modeinfo_default(void) {
#ifdef TARGET_PC98
    vga2_alpha_base.width = 80;                         /* always 80, even in 40 column mode. see notes above. */
    vga2_alpha_base.height = 25;                        /* assume 25, but code below will poke into DOS kernel for more info */

    /* Poking into the DOS kernel directly is bad, but on PC-98 MS-DOS is a very common practice by
     * games and applications to determine cursor position and screen size.
     *
     * If it matters, perhaps this code could instead poke into the BIOS data area instead. */
    {
        /* 0x60:0x112 = ANSI scroll region lower limit (last line number).
         *              Normally this is the last line before the function key row.
         *              On a 25-line text display with the function key row visible this value would be 23.
         *              This code will return the wrong value if a smaller scroll region has been defined
         *              through the ANSI driver (INT DCh or otherwise).
         *              If your program clears everything away at startup and never sets a scroll region
         *              this code should work OK.
         *              Using this value is very common with PC-98 games. */
        register unsigned char sh = *vga2_dosseg_b(0x112) + 1;
        /* 0x60:0x111 = Function key row display status. 0x01 if visible, 0x00 if not.
         *              If it's visible then one more line needs to be counted. */
        if (*vga2_dosseg_b(0x111) != 0) sh++;

        /* if the result seems reasonable then accept it */
        if (sh >= 12) vga2_alpha_base.height = sh;
    }
#else
    if ((vga2_flags & (VGA2_FLAG_EGA|VGA2_FLAG_MCGA|VGA2_FLAG_VGA)) != 0u) {
        /* EGA/VGA: Could be mono or color.
         * NTS: We could read it back from hardware on VGA, but that's an extra IF statement
         *      and I think it would be better for executable size reasons to combine EGA/VGA
         *      case of inferring it from BIOS data.
         *
         * NTS: One would think you'd infer this from the equipment word, but most software,
         *      including Microsoft software, is determining B800 vs B000 by reading the
         *      video mode and assuming B000 if mode 7.
         *
         *      This makes sense since on original CGA hardware mode 7 doesn't exist, and
         *      on MDA, only mode 7 exists.
         *
         *      (well actually the 5150 BIOS I have with CGA does allow mode 7 *bug*, in which
         *      case the CGA output is programmed to MDA timing!)
         *
         * NTS: MCGA only supports the color modes and will NOT allow setting mode 7. It will
         *      only respond to CGA memory and I/O ranges. It has 64KB of video memory at
         *      A0000h and the last 32KB of that 64KB is mirrored at B8000h for CGA compat. */
        vga2_alpha_ptrsz_set(0x8000u); /* 32KB */
        vga2_alpha_ptr_set((vga2_get_int10_current_mode() == 7u) ? 0xB000u : 0xB800u);
    }
    else {
        /* CGA/MCGA: Color, always */
        /* MDA/Hercules: Mono, always */
        /* MCGA is hard-wired to map 64KB of RAM to A0000-AFFFF at all times,
         * with B0000-B7FFF unmapped and B8000-BFFFF an alias of the last 32KB. CGA and MCGA cannot respond to B000h. */
        /* Remember that the original CGA had only 16KB of RAM (16KB repeated twice in the 32KB memory region).
         * The 16KB limit is also on the PCjr. CGA clone cards like the ColorPlus will have the full 32KB,
         * but if your code supports that the extra libary code to set it up will change this to 32KB.
         *
         * CGA graphics modes are wired so that 200-line graphics exist as two 100-line 8KB framebuffers interleaved
         * together, even/odd scanlines (bit 0 of the row counter as bit 13 of the address), within the 16KB.
         *
         * There is only 4KB of RAM on the MDA. Extra libary code to support the Hercules graphics cards will
         * change this to 32KB as needed if your code wants to support HGC graphics and hardware. */
        if (vga2_flags & VGA2_FLAG_CGA) {
            /* CGA: 16KB at B8000h */
            vga2_alpha_ptrsz_set(0x4000u);
            vga2_alpha_ptr_set(0xB800u);
        }
        else {
            /* MDA: 4KB at B0000h */
            vga2_alpha_ptrsz_set(0x1000u);
            vga2_alpha_ptr_set(0xB000u);
        }
    }

    vga2_alpha_base.width = *vga2_bda_w(0x4A); /* number of text columns in active display mode */
    {
        /* NTS: Believe it or not IBM BIOSes prior to about 1985 (in the BIOS listings) do not mention
         *      anything about a number of rows value in the BIOS data area. That value is typically
         *      zero on those systems. */
        register unsigned char c = *vga2_bda_b(0x84); /* number of rows minus 1 */
        if (c == 0u) c = 24u; /* assume 80x25 which is reasonable for CGA/MDA/PCjr */
        vga2_alpha_base.height = c + 1u;
    }
#endif
}

