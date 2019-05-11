#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1
dostiny=1 # MS-DOS tiny model
doshuge=1

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86s/test.exe ::test86.exe
    mcopy -i test.dsk dos86l/test.exe ::test86l.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe
    mcopy -i test.dsk ../../dos4gw.dat ::dos4gw.exe
    mcopy -i test.dsk dos86s/testpnp.exe ::tstp86.exe
    mcopy -i test.dsk dos86l/testpnp.exe ::tstp86l.exe
    mcopy -i test.dsk dos386f/testpnp.exe ::tstp386.exe
    cp test.dsk test2.dsk
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

