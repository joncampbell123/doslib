#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dostiny=1 # MS-DOS tiny model
doshuge=1 # MS-DOS huge model
dospc98=1 # MS-DOS PC-98
win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
os216=1 # OS/2 16-bit 286
os232=1 # OS/2 32-bit 386
win386=1
win38631=1

if [ "$1" == "clean" ]; then
    do_clean
    rm -fv test.dsk
    rm -Rfv linux-host
    exit 0
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk ../../debug.exx ::debug.exe
    mcopy -i test.dsk dos86s/ansi.exe ::ansi86.exe
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
    mcopy -i test.dsk win386/test.exe ::tstw386.exe
    mcopy -i test.dsk win38631/test.exe ::tw38631.exe
    mcopy -i test.dsk dos86s/testsmrt.exe ::tsmrt86.exe
    mcopy -i test.dsk dos386f/testsmrt.exe ::tsmrt386.exe
    mcopy -i test.dsk dos86s/hexstdin.exe ::hexstdin.exe
    mcopy -i test.dsk dos86s/hexstdi2.exe ::hexstdi2.exe
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

