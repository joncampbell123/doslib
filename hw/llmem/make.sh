#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1
doshuge=1
dostiny=1 # MS-DOS tiny model

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk v86kern.map
    rm -f bochstst/win95.dsk bochstst/hdd.dsk
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe

    mcopy -i test.dsk dos86c/test.exe ::test86c.exe
    mcopy -i test.dsk dos86s/test.exe ::test86s.exe
    mcopy -i test.dsk dos86m/test.exe ::test86m.exe
    mcopy -i test.dsk dos86l/test.exe ::test86l.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe

    # bootable win95 rescue disk with test program
    gunzip -c -d win95.dsk.gz >bochstst/win95.dsk
    gunzip -c -d hdd.dsk.gz >bochstst/hdd.dsk
    gunzip -c -d emm386.exe.gz >bochstst/emm386.exe
    mdel -i bochstst/win95.dsk ::test86.exe
    mdel -i bochstst/win95.dsk ::v86kern.com
    mdel -i bochstst/win95.dsk ::v86kern2.com
    mdel -i bochstst/win95.dsk ::scandisk.exe
    mdel -i bochstst/win95.dsk ::scandisk.ini
    mcopy -i bochstst/win95.dsk dos86c/test.exe ::test86c.exe
    mcopy -i bochstst/win95.dsk dos86s/test.exe ::test86s.exe
    mcopy -i bochstst/win95.dsk dos86m/test.exe ::test86m.exe
    mcopy -i bochstst/win95.dsk dos86l/test.exe ::test86l.exe
    mcopy -i bochstst/win95.dsk dos386f/test.exe ::test386.exe
    mcopy -i bochstst/win95.dsk dos386f/dos4gw.exe ::dos4gw.exe
    mcopy -i bochstst/win95.dsk bochstst/emm386.exe ::emm386.exe
    mdel -i bochstst/win95.dsk ::config.sys
    mcopy -i bochstst/win95.dsk config.sys ::config.sys
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

