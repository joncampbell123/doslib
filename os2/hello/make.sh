#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

os216=1 # OS/2 16-bit 286
os232=1 # OS/2 32-bit 386

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk v86kern.map
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk os2w2l/hello.exe ::helos216.exe
    mcopy -i test.dsk os2d3f/hello.exe ::helos232.exe

    mcopy -i test.dsk os2w2l/hellopm.exe ::hpmos216.exe
    mcopy -i test.dsk os2d3f/hellopm.exe ::hpmos232.exe
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

