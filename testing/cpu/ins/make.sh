#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

# use large memory model so the CPU tests have the maximum flexibility in 16-bit real mode to do what they need
build_list="dos86l dos386f"
# PC-98 targets too
build_list="$build_list d9886l d98386f"

if [ "$1" == "clean" ]; then
    do_clean
    exit 0
fi

if [ "$1" == "disk" ]; then
    true
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    #make_buildlist
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

