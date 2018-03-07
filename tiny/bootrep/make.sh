#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dospc98=1 # PC-98

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv boot*.dsk
    exit 0
fi

if [ "$1" == "disk" ]; then
    dd if=/dev/zero          of=bootibm.dsk bs=512 count=2880 || exit 1
    dd if=dos86s/bootrep.com of=bootibm.dsk bs=512 conv=notrunc || exit 1

    dd if=/dev/zero          of=bootpc98.dsk bs=512 count=2880 || exit 1
    dd if=d9886s/bootrep.com of=bootpc98.dsk bs=512 conv=notrunc || exit 1
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

