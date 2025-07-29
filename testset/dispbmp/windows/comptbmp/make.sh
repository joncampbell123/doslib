#!/bin/bash
rel=../../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
win386=1
win38631=1

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk test2.dsk pcjrtest.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd pcx2vrl png2vrl pcxsscut vrl2vrs vrsdump vrldbg *.o
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

