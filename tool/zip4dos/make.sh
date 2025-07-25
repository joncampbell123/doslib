#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
doshuge=1 # MS-DOS huge model
dospc98=1 # MS-DOS PC-98
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.

if [ "$1" == "clean" ]; then
    do_clean
    make clean
    rm -fv test.dsk
    rm -Rfv linux-host
    exit 0
fi

if [ "$1" == "disk" ]; then
    true
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
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

