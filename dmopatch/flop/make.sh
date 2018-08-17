#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    do_clean
    rm -Rfv dos86s
    exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    build_list="dos86s"
    what=all

    for name in $build_list; do
        do_wmake $name "$what" || exit 1
        bat_wmake $name "$what" || exit 1
    done

    end_bat
fi

