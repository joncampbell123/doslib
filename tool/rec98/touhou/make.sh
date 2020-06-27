#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # DOS executable will not touch hardware or BIOS interfaces, therefore should work on either IBM PC or PC-98
doshuge=1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.

if [ "$1" == "clean" ]; then
    do_clean
    rm -Rfv linux-host
    rm -v *.test.{asm,bin,bmp,c}
    rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
#    mcopy -i test.dsk dos86s/dosamp.exe ::amp86s.exe
#    mcopy -i test.dsk dos86l/dosamp.exe ::amp86l.exe
#    mcopy -i test.dsk dos386f/dosamp.exe ::amp386.exe
#    mcopy -i test.dsk win32s3/dosamp.exe ::amp32s.exe
#    mcopy -i test.dsk win32/dosamp.exe ::amp32.exe
#    mcopy -i test.dsk winnt/dosamp.exe ::ampnt.exe
#    mcopy -i test.dsk win313l/dosamp.exe ::amp16.exe
#    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
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

