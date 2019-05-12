#!/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    do_clean
    rm -Rfv win313l
    exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    allow_build_list="win313l"

    make_buildlist
    begin_bat

    what=all
    if [ x"$2" != x ]; then what="$2"; fi
    if [ x"$3" != x ]; then build_list="$3"; fi

    for name in $build_list; do
        do_wmake $name "$what" || exit 1
        bat_wmake $name "$what" || exit 1
    done

# TODO: temporary disabled, need to fix
    # copy the result into BIN
    cp -vu win313l/dboxmpi.drv ../bin/win95/dboxmpi.drv
#    cp -vu win313l/dboxmpi.vxd ../bin/win95/dboxmpi.vxd
    cp -vu win313l/dboxmpi.drv ../bin/win98/dboxmpi.drv
#    cp -vu win313l/dboxmpi.vxd ../bin/win98/dboxmpi.vxd
    cp -vu win313l/dboxmpi.drv ../bin/winme/dboxmpi.drv
#    cp -vu win313l/dboxmpi.vxd ../bin/winme/dboxmpi.vxd

    end_bat
fi

