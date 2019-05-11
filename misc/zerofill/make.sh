#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1
doshuge=1
dospc98=1 # MS-DOS PC98

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk v86kern.map
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86c/zerofill.exe ::zfil86c.exe
    mcopy -i test.dsk dos86s/zerofill.exe ::zfil86s.exe
    mcopy -i test.dsk dos86m/zerofill.exe ::zfil86m.exe
    mcopy -i test.dsk dos86l/zerofill.exe ::zfil86l.exe
    mcopy -i test.dsk dos386f/zerofill.exe ::zfil386.exe
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
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

