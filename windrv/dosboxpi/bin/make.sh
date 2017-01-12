#!/usr/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    true
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk install.dsk || exit 1

    mmd   -i install.dsk                    ::win30
    mcopy -i install.dsk win30/dboxmpi.drv  ::win30/dboxmpi.drv
    mcopy -i install.dsk win30/oemsetup.inf ::win30/oemsetup.inf

    mmd   -i install.dsk                    ::win31
    mcopy -i install.dsk win31/dboxmpi.drv  ::win31/dboxmpi.drv
    mcopy -i install.dsk win31/oemsetup.inf ::win31/oemsetup.inf
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    true
fi

