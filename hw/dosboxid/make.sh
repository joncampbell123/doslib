#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dostiny=1 # MS-DOS tiny model
doshuge=1 # MS-DOS huge
dospc98=1 # MS-DOS PC98
win30=1 # Windows 3.0
win31=1 # Windows 3.1
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
win386=1 # Windows 3.0 + Watcom 386 extender
win38631=1 # Windows 3.1 + Watcom 386 extender

# WARNING: Test programs that run in the Win32 environment are provided,
#          but should only be run under Windows 3.1, 95, 98, or ME.
#          They will not work under Windows NT, because they will use
#          this library in a form that expects to poke directly at I/O
#          ports. Additionally, because of the preemptive multitasking
#          nature of Windows 95/98/ME, the test programs WILL work but
#          might conflict badly if more than one task is directly poking
#          at the I/O ports (there is not yet any driver to virtualize
#          the I/O ports to share across processes).
#
#          Windows targets should be using a DLL to access this device,
#          rather than direct linking to this library. The DLL method
#          would allow the program to use a general API while leaving
#          the actual communication up to a target-specific method,
#          be it direct I/O ports in Windows 3.x to a VXD device driver
#          API in Windows 9x/ME to a kernel driver via DeviceIoControl
#          in Windows NT. A 16-bit and 32-bit DLL will be provided with
#          different names for each bitness.

if [ "$1" == "clean" ]; then
    do_clean
    rm -Rfv win3???_drv win3???_drvn
    rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
    exit 0
fi

if [ "$1" == "disk" ]; then
    gunzip -c -d win95.dsk.gz >test.dsk || exit 1
    mcopy -i test.dsk dos86m/test.exe ::test86m.exe
    mcopy -i test.dsk dos86l/test.exe ::test86.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
fi

if [ "$1" == "disk2" ]; then
    gunzip -c -d msdos622.dsk.gz >test.dsk || exit 1
    mcopy -i test.dsk dos86m/test.exe ::test86m.exe
    mcopy -i test.dsk dos86l/test.exe ::test86.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    also_build_list="win300l win312l"
    make_buildlist
    begin_bat

    what=all
    if [ x"$2" != x ]; then what="$2"; fi
    if [ x"$3" != x ]; then build_list="$3"; fi

    for name in $build_list; do
        do_wmake $name "$what" || exit 1
        bat_wmake $name "$what" || exit 1
    done

    end_bat
fi

