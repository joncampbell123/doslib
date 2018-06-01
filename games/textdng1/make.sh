#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

#dos=1

if [ "$1" == "clean" ]; then
    do_clean
    rm -Rfv linux-host
    rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86s/game.exe ::game.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    export allow_build_list="dos86s dos86l"
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

    for target in dos86{s,l}; do
        cp -vu level??.txt ./$target/ || exit 1
    done
fi

