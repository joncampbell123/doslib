#!/usr/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    rm -fv win??/*.drv
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk install.dsk || exit 1

    mmd   -i install.dsk                    ::win10
    mcopy -i install.dsk win10/ps2mouse.drv  ::win10/ps2mouse.drv
    mcopy -i install.dsk win10/README.TXT   ::win10/readme.txt

    mmd   -i install.dsk                    ::win20
    mcopy -i install.dsk win20/ps2mouse.drv  ::win20/ps2mouse.drv
    mcopy -i install.dsk win20/oemsetup.inf ::win20/oemsetup.inf

    mmd   -i install.dsk                    ::win30
    mcopy -i install.dsk win30/ps2mouse.drv  ::win30/ps2mouse.drv
    mcopy -i install.dsk win30/oemsetup.inf ::win30/oemsetup.inf

    mmd   -i install.dsk                    ::win31
    mcopy -i install.dsk win31/ps2mouse.drv  ::win31/ps2mouse.drv
    mcopy -i install.dsk win31/oemsetup.inf ::win31/oemsetup.inf
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    true
fi

