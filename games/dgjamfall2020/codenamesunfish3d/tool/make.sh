#!/bin/bash
rel=../../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS

if [ "$1" == "clean" ]; then
    do_clean
    rm -v pngmatchpal
    exit 0
fi

if [ "$1" == "disk" ]; then
    true
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    true
fi

