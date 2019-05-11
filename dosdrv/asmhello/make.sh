#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk
    exit 0
fi

if [ "$1" == "disk" ]; then
    # bootable win95 rescue disk with test program
    gunzip -c -d test.dsk.gz >test.dsk
    mcopy -i test.dsk config.sys ::config.sys
    mcopy -i test.dsk dos86s/hello.sys ::hello.sys
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

