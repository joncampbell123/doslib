#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh
if [ "$1" == "clean" ]; then
    rm -fv *.obj *.lib *.exe *.com *.map
    exit 0
else
    wmake -h -f watcom_c.mak
fi

