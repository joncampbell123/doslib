#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos86s/test.exe ::test86.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe
    mcopy -i test.dsk dos86s/testbext.exe ::testbext.exe
    mcopy -i test.dsk dos86s/tsthimem.exe ::tsthimem.exe
    mcopy -i test.dsk dos86s/testemm.exe ::testemm.exe
    mcopy -i test.dsk dos86s/tstbiom.exe ::tstbiom.exe
    mcopy -i test.dsk dos86s/lol.exe ::lol86.exe
    mcopy -i test.dsk dos386f/lol.exe ::lol386.exe
    mcopy -i test.dsk dos86s/tstlp.exe ::tlp86.exe
    mcopy -i test.dsk dos386f/tstlp.exe ::tlp386.exe
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
    mcopy -i test.dsk win302l/test.exe ::test30.exe
    mcopy -i test.dsk win302l/cr3.exe ::cr3_30.exe
    mcopy -i test.dsk win313l/test.exe ::test31.exe
    mcopy -i test.dsk win313l/cr3.exe ::cr3_31.exe
    mcopy -i test.dsk win32s3/test.exe ::test32s.exe
    mcopy -i test.dsk win32/test.exe ::test32.exe
    mcopy -i test.dsk win32/cr3.exe ::cr3_32.exe
    mcopy -i test.dsk dos86s/testdpmi.exe ::dpmi86s.exe
    mcopy -i test.dsk dos86l/testdpmi.exe ::dpmi86l.exe
    mcopy -i test.dsk os2w2l/test.exe ::tstos216.exe
    mcopy -i test.dsk os2w2l/cr3.exe ::cr3os216.exe
    mcopy -i test.dsk os2d3f/test.exe ::tstos232.exe
    mcopy -i test.dsk os2d3f/cr3.exe ::cr3os232.exe
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

