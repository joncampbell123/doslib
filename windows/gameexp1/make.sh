#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

win10=1 # Windows 1.0
win20=1 # Windows 2.0
win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
win386=1 # Windows 3.0 + Watcom win386
win38631=1 # Windows 3.1 + Watcom win386

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk v86kern.map game*.iso
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk winnt/hello.exe ::helwinnt.exe
fi

if [ "$1" == "cdrom" ]; then
    mkisofs -J -v -volid 'GAME2025_31' -o gamewin31.iso win313l || exit 1
    mkisofs -J -v -volid 'GAME2025_32s' -o gamewin32s.iso win32s3 || exit 1
    mkisofs -J -v -volid 'GAME2025_95' -o gamewin95.iso win32 || exit 1
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

