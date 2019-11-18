#!/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    do_clean
    make clean
    rm -fv test.dsk
    rm -Rfv linux-host
    exit 0
fi

if [ "$1" == "disk" ]; then
    true
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    true
fi

