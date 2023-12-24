#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

# FIXME: Only because DOSBox + MSVC++ keep saving filenames in caps
#mv -vn RESOURCE.H resource.h
#mv -vn HELLO.RC hello.rc
#mv -vn HELLO.ICO hello.ico

win20=1 # Windows 2.0
win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
#win386=1 # Windows 3.1 + Watcom win386        <- FIXME: Ok great, it produces some proprietary EXE with an 'MQ' signature, how the fuck do I run it?

if [ "$1" == "clean" ]; then
    do_clean
    rm -f *.aps # Damn you Visual C++ AppStudio
    rm -fv test.dsk v86kern.map
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk win300l/hello.exe ::hel86_30.exe
    mcopy -i test.dsk win302l/hello.exe ::he286_30.exe
    mcopy -i test.dsk win303l/hello.exe ::he386_30.exe
    mcopy -i test.dsk win300l/hello3.exe ::3el86_30.exe
    mcopy -i test.dsk win302l/hello3.exe ::3e286_30.exe
    mcopy -i test.dsk win303l/hello3.exe ::3e386_30.exe
    mcopy -i test.dsk win300l/hello4.exe ::4el86_30.exe
    mcopy -i test.dsk win302l/hello4.exe ::4e286_30.exe
    mcopy -i test.dsk win303l/hello4.exe ::4e386_30.exe

    mcopy -i test.dsk win312l/hello.exe ::he286_31.exe
    mcopy -i test.dsk win313l/hello.exe ::he386_31.exe
    mcopy -i test.dsk win312l/hello3.exe ::3e286_31.exe
    mcopy -i test.dsk win313l/hello3.exe ::3e386_31.exe
    mcopy -i test.dsk win312l/hello4.exe ::4e286_31.exe
    mcopy -i test.dsk win313l/hello4.exe ::4e386_31.exe

    mcopy -i test.dsk win32s3/hello.exe ::hewin32s.exe
    mcopy -i test.dsk win32s3/hello3.exe ::3ewin32s.exe
    mcopy -i test.dsk win32s3/hello4.exe ::4ewin32s.exe

    mcopy -i test.dsk win32/hello.exe ::helwin32.exe
    mcopy -i test.dsk win32/hello3.exe ::3elwin32.exe
    mcopy -i test.dsk win32/hello4.exe ::4elwin32.exe

    mcopy -i test.dsk winnt/hello.exe ::helwinnt.exe
    mcopy -i test.dsk winnt/hello3.exe ::3elwinnt.exe
    mcopy -i test.dsk winnt/hello4.exe ::4elwinnt.exe
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

