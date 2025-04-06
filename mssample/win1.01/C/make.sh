#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

# THESE SAMPLES CANNOT COMPILE AS ANYTHING OTHER THAN 16-BIT WINDOWS CODE
win10=1 # Windows 1.0
win20=1 # Windows 2.0
win30=1 # Windows 3.0
win31=1 # Windows 3.1

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk v86kern.map
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
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

