#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
dostiny=1 # MS-DOS tiny model
doshuge=1 # MS-DOS huge model
dospc98=1 # MS-DOS PC98
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
    rm -Rfv dos86
    rm -fv test.dsk gr_add.dsk v86kern.map
    rm -f bochstst/win95.dsk bochstst/hdd.dsk
    exit 0
fi

if [ "$1" == "disk" ]; then
    # boot data disk
    make_msdos_data_disk test.dsk || exit 1
    mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe

    #mcopy -i test.dsk grtest.bat ::grtest.bat

    mcopy -i test.dsk dos86l/grind.exe ::grind86.exe
    mcopy -i test.dsk dos386f/grind.exe ::grin386.exe
    mcopy -i test.dsk dos86l/gr_add.exe ::gradd16.exe
    mcopy -i test.dsk dos386f/gr_add.exe ::gradd32.exe

    mcopy -i test.dsk dos86l/test.exe ::test86l.exe
    mcopy -i test.dsk dos386f/test.exe ::test386.exe

    mcopy -i test.dsk win313l/test.exe ::tstw313l.exe
    mcopy -i test.dsk win32s3/test.exe ::tstw32s.exe
    mcopy -i test.dsk win32/test.exe ::tstw9x.exe
    mcopy -i test.dsk win386/test.exe ::tstw386.exe

    mcopy -i test.dsk os2w2l/test.exe ::tstos216.exe
    mcopy -i test.dsk os2d3f/test.exe ::tstos232.exe

    mcopy -i test.dsk dos86l/mmx.exe ::mmx86l.exe
    mcopy -i test.dsk dos386f/mmx.exe ::mmx386.exe
    mcopy -i test.dsk win386/mmx.exe ::mmxw386.exe

    mcopy -i test.dsk dos386f/gdtlist.exe ::gdtlist.exe
    mcopy -i test.dsk win386/gdtlist.exe ::gdtw386.exe

#   mcopy -i test.dsk win313l/mmx.exe ::mmxw313l.exe

#   mcopy -i test.dsk os2w2l/mmx.exe ::mmxo216.exe
#   mcopy -i test.dsk os2d3f/mmx.exe ::mmxo232.exe

    mcopy -i test.dsk dos86l/sse.exe ::sse86l.exe
    mcopy -i test.dsk dos386f/sse.exe ::sse386.exe

    mcopy -i test.dsk win313l/sse.exe ::ssew313l.exe
    mcopy -i test.dsk win32s3/sse.exe ::ssew32s.exe
#   mcopy -i test.dsk win32/sse.exe ::ssew9x.exe
#   mcopy -i test.dsk win386/sse.exe ::ssew386.exe

#   mcopy -i test.dsk os2w2l/sse.exe ::sseo216.exe
#   mcopy -i test.dsk os2d3f/sse.exe ::sseo232.exe

#   mcopy -i test.dsk dos86l/sseoff.exe ::sseo86l.exe
#   mcopy -i test.dsk dos386f/sseoff.exe ::sseo386.exe

#   mcopy -i test.dsk os2w2l/sseoff.exe ::sseoo16.exe
#   mcopy -i test.dsk os2d3f/sseoff.exe ::sseoo32.exe

#   mcopy -i test.dsk win313l/sseoff.exe ::ssow313l.exe

    mcopy -i test.dsk dos86l/reset.exe ::rese86l.exe
    mcopy -i test.dsk dos386f/reset.exe ::rese386.exe

    mcopy -i test.dsk dos86l/rdtsc.exe ::rdts86l.exe
    mcopy -i test.dsk dos386f/rdtsc.exe ::rdts386.exe

    mcopy -i test.dsk win313l/rdtsc.exe ::rdtw313l.exe
    mcopy -i test.dsk win32s3/rdtsc.exe ::rdtw32s.exe
    mcopy -i test.dsk win32/rdtsc.exe ::rdtw32.exe
    mcopy -i test.dsk win386/rdtsc.exe ::rdtw386.exe

    mcopy -i test.dsk os2w2l/rdtsc.exe ::rdto216.exe
    mcopy -i test.dsk os2d3f/rdtsc.exe ::rdto232.exe

    mcopy -i test.dsk dos86s/prot286.com ::prot286.com
    mcopy -i test.dsk dos86s/prot386.com ::prot386.com
    mcopy -i test.dsk dos86s/tss.com ::tss.com
    mcopy -i test.dsk dos86s/v86.com ::v86.com
    mcopy -i test.dsk dos86s/v86kern.com ::v86kern.com
    mcopy -i test.dsk dos86s/v86kern2.com ::v86kern2.com
    mcopy -i test.dsk dos86s/tssring.com ::tssring.com
    mcopy -i test.dsk dos86s/alignchk.com ::alignchk.com
    mcopy -i test.dsk dos86s/protvcpi.com ::protvcpi.com
    mcopy -i test.dsk dos86s/protdpmi.com ::protdpmi.com

    mcopy -i test.dsk dos86c/apic.exe ::apic86c.exe
    mcopy -i test.dsk dos86s/apic.exe ::apic86s.exe
    mcopy -i test.dsk dos86m/apic.exe ::apic86m.exe
    mcopy -i test.dsk dos86l/apic.exe ::apic86l.exe
    mcopy -i test.dsk dos386f/apic.exe ::apic386.exe

    # bootable win95 rescue disk with test program
    gunzip -c -d win95.dsk.gz >bochstst/win95.dsk
    gunzip -c -d hdd.dsk.gz >bochstst/hdd.dsk
    gunzip -c -d emm386.exe.gz >bochstst/emm386.exe
    mdel -i bochstst/win95.dsk ::test86.exe
    mdel -i bochstst/win95.dsk ::v86kern.com
    mcopy -i bochstst/win95.dsk dos86s/v86kern.com ::v86kern.com
    #mdel -i bochstst/win95.dsk ::v86kern2.com
    mdel -i bochstst/win95.dsk ::scandisk.exe
    mdel -i bochstst/win95.dsk ::scandisk.ini
    mcopy -i bochstst/win95.dsk dos86s/v86kern2.com ::v86kern2.com
    mcopy -i bochstst/win95.dsk dos86s/protvcpi.com ::protvcpi.com
    mcopy -i bochstst/win95.dsk dos86l/test.exe ::test86l.exe
    mcopy -i bochstst/win95.dsk dos386f/dos4gw.exe ::dos4gw.exe
    mcopy -i bochstst/win95.dsk bochstst/emm386.exe ::emm386.exe
    mdel -i bochstst/win95.dsk ::config.sys
    mcopy -i bochstst/win95.dsk config.sys ::config.sys

    # boot grind disk GR_ADD
    gunzip -c -d msdos5m.dsk.gz >gr_add.dsk
    mcopy -i gr_add.dsk dos386f/dos4gw.exe ::dos4gw.exe

    mcopy -i gr_add.dsk dos86l/gr_add.exe ::gradd16.exe
    mcopy -i gr_add.dsk dos386f/gr_add.exe ::gradd32.exe
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

