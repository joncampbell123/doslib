#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dostiny=1 # MS-DOS tiny model
doshuge=1 # MS-DOS huge model
dospc98=1 # MS-DOS PC-98
win30=1 # Windows 3.0
win31=1 # Windows 3.1

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk test98.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86l/test.exe ::test86l.exe
    mcopy -i test.dsk dos86s/test.exe ::test86s.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe
    mcopy -i test.dsk test1_22.wav ::test1_22.wav
    gunzip -c -d ../necpc98/necpc98.fd0.gz >test98.dsk
    mcopy -i test98.dsk d9886l/test.exe ::test98l.exe
    mcopy -i test98.dsk d9886s/test.exe ::test98s.exe
    mcopy -i test98.dsk d98386f/test.exe ::test98f.exe
    mcopy -i test98.dsk test1_22.wav ::test1_22.wav
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

