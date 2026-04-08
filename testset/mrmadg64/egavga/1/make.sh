#!/bin/bash
rel=../../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    if [ -d dos86t ]; then
        rm -v dos86t/*.com
        rm -v dos86t/*.COM
        rm -v dos86t/*.lst
        rm -v dos86t/*.LST
        rm -v dos86t/*.map
        rm -v dos86t/*.MAP
        rm -v dos86t/*.obj
        rm -v dos86t/*.OBJ
        rmdir dos86t
    fi
    rm -v *.obj *.OBJ
    rm -v *.lst *.LST
    rm -v *.map *.MAP
    rm -v *.com *.COM
    rm -fv test.dsk
    rm -Rfv linux-host
    exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    mkdir -p dos86t || exit 1
    dosbox-x --conf build.conf || exit 1
    [ -f dos86t/TEST1.COM ] || exit 1
    [ -f dos86t/TEST2.COM ] || exit 1
    [ -f dos86t/TEST3.COM ] || exit 1
    [ -f dos86t/TEST4.COM ] || exit 1
fi

