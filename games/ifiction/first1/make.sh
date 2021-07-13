#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=0
also_build_list="dos386f win32s3"

if [ "$1" == "clean" ]; then
    do_clean
    rm -f ifictdos.exe ifictw31.exe *.exe.map ifictsdl2 *.o dos4gw.exe
    exit 0
fi

if [ "$1" == "disk" ]; then
    rm -f test.iso
    mkisofs -o test.iso -J .
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

