Interactive Fiction #1

An experiment in interactive fiction, in the style of mid 1990s point and click
"interactive" CD-ROM type games.

The game runs in 640x480 256-color mode.
Binaries are provided for MS-DOS, Windows 3.1/95
and Mac OS X (unsigned) using SDL2.


Run the version appropriate for your setup:

IFICTDOS.EXE                  32-bit MS-DOS version
IFICTW31.EXE                  32-bit Windows 3.1/95 version

*.app bundles are provided for Intel and ARM based Mac OS X systems.


MS-DOS version:
- Minimum requirements:
    Intel 386 or higher
    4MB of RAM
    MS-DOS mouse driver required
    SVGA graphics card capable of 640x480 256-color mode
    VESA BIOS extensions 1.0 or higher
- Recommended:
    Intel 486 or Pentium processor at 66MHz or higher (because mid 1990s PC gaming, right?)
    8MB of RAM
    MS-DOS mouse driver (CuteMouse recommended)
    PCI graphics card
    VESA BIOS extensions 2.0 with linear framebuffer

If this game is run in DOSBox-X with the "dosbox integration device", the game
will emit debug output to your DOSBox log file as it runs.

This game will work with older SVGA cards (ISA/EISA) that offer only the non-linear
bank switching access to video memory at a slight reduction of performance.

This game runs perfectly fine within a Windows "DOS box", but you will need to
run a mouse driver within the DOS box if one is not already resident in MS-DOS
before starting Windows. CuteMouse (CTMOUSE.EXE) is provided for your convenience.

This game works with DOS4GW.EXE, but may run better with DOS32A.EXE instead.
Also tested under CWSDPMI.EXE.


Windows 3.1/95 version:
- Minimum requirements:
    Windows 3.1 or higher
    Intel 386 or higher
    4MB of RAM
    Mouse driver (most Windows systems have one installed)
    Video driver configured to 640x480 or higher
    Microsoft Win32s (Win32 subsystem) for Windows 3.1
- Recommended:
    Windows 95 or higher
    Intel 486 or Pentium processor at 100MHz or higher
    8MB of RAM
    PCI graphics card and driver with hardware acceleration enabled
    Video driver configured to 640x480 or higher set to 256-color mode

As the game uses the base level Win32 API, it should be perfectly fine to
run the game on later versions of Windows including Windows ME, 2000, XP,
Vista, 7, 8, 10, and probably even Windows 11.

WARNING: You *can* run this game in 640x480 16-color mode, or even 640x480
         monochrome (1-bit) mode, but you will get very bad performance and
         a suboptimal, barely usable visual rendition of the game's graphics.

         This game is optimized for 256-color mode, some slight performance
         degredation may occur if your desktop is configured for
         highcolor/truecolor display mode.


Mac OS X builds:
- Minimum requirements:
    Mac OS X Big Sur 11.0 or higher

This game's .app bundle is signed using an "ad-hoc" signature, it is not
vetted or signed by Apple in any way. Gatekeeper should notify you of this
upon running and ask if you should like to run anyway. I have no ill intent
in developing this game and I wish your system no harm.

