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
win386=1
win38631=1

if [ "$1" == "clean" ]; then
    do_clean
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
    make_buildlist
    begin_bat

    what=all
    if [ x"$2" != x ]; then what="$2"; fi

    for name in $build_list; do
        do_wmake $name "$what" || exit 1
        bat_wmake $name "$what" || exit 1
    done

    end_bat
fi

