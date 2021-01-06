#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

#dos=1 # MS-DOS
#dostiny=1 # MS-DOS tiny model
#doshuge=1 # MS-DOS huge model
#dospc98=1 # MS-DOS PC-98
#win30=1 # Windows 3.0
#win31=1 # Windows 3.1
#winnt=1 # Windows NT
#win32=1 # Windows 9x/NT/XP/Vista/etc.
#win32s=1 # Windows 3.1 + Win32s
#os216=1 # OS/2 16-bit 286
#os232=1 # OS/2 32-bit 386
#win386=1
#win38631=1

if [ "$1" == "clean" ]; then
    do_clean
    rm -f win95.dsk drva.asm drva2.asm
    rm -Rf linux-host
    exit 0
fi

if [ "$1" == "disk" ]; then
    # bootable win95 rescue disk with test program
    gunzip -c -d win95.dsk.gz >win95.dsk
    mcopy -i win95.dsk dos86t/test.sys ::drv.sys
    mcopy -i win95.dsk dos86t/test.exe ::drv.exe
    mcopy -i win95.dsk dos86t/test2.sys ::drv2.sys
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
#    make_buildlist
    build_list=dos86t
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

