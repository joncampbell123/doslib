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

chk_built() {
    count=0
    if [ -f dos86t/TEST1.COM ]; then count=$(($count+1)); fi
    if [ -f dos86t/TEST2.COM ]; then count=$(($count+1)); fi
    if [ -f dos86t/TEST3.COM ]; then count=$(($count+1)); fi
    if [ -f dos86t/TEST4.COM ]; then count=$(($count+1)); fi
    if [ $(($count >= 4)) == 1 ]; then return 0; fi
    return 1
}

if [[ "$1" == "build" || "$1" == "" ]]; then
    if chk_built; then
        true
    else
        mkdir -p dos86t || exit 1
        dosbox-x --conf build.conf || exit 1
        chk_built || exit 1
    fi
fi

