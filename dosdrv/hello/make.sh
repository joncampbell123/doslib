#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh
if [ "$1" == "clean" ]; then
    rm -Rfv dos86s
    rm -fv *.obj *.lib *.exe *.com *.bin drv.map win95.dsk drva.asm
    exit 0
elif [ "$1" == "disk" ]; then
    # bootable win95 rescue disk with test program
    gunzip -c -d win95.dsk.gz >win95.dsk
    mcopy -i win95.dsk dos86s/drv.exe ::drv.exe
else
    wmake -h -f watcom_c.mak
fi

