#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dospc98=1 # MS-DOS PC-98

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk win95.dsk
    rm -Rfv linux-host
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86s/remsrv.exe ::remsrv.exe

    # bootable win95 rescue disk with test program
    gunzip -c -d win95.dsk.gz >win95.dsk
    mcopy -i win95.dsk dos86s/remsrv.exe ::remsrv.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    exit 0 # fixme
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

