#!/usr/bin/bash
rel=../../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
    true
fi

if [ "$1" == "disk" ]; then
    make_msdos_data_disk install.dsk || exit 1

    mmd   -i install.dsk                    ::win10
    mcopy -i install.dsk win10/dboxmpi.drv  ::win10/dboxmpi.drv
    mcopy -i install.dsk win10/README.TXT   ::win10/readme.txt

    mmd   -i install.dsk                    ::win20
    mcopy -i install.dsk win20/dboxmpi.drv  ::win20/dboxmpi.drv
    mcopy -i install.dsk win20/oemsetup.inf ::win20/oemsetup.inf

    mmd   -i install.dsk                    ::win30
    mcopy -i install.dsk win30/dboxmpi.drv  ::win30/dboxmpi.drv
    mcopy -i install.dsk win30/oemsetup.inf ::win30/oemsetup.inf

    mmd   -i install.dsk                    ::win31
    mcopy -i install.dsk win31/dboxmpi.drv  ::win31/dboxmpi.drv
    mcopy -i install.dsk win31/oemsetup.inf ::win31/oemsetup.inf

    mmd   -i install.dsk                    ::win95
    mcopy -i install.dsk win95/dboxmpi.drv  ::win95/dboxmpi.drv
    mcopy -i install.dsk win95/dboxmpi.inf  ::win95/dboxmpi.inf

    mmd   -i install.dsk                    ::win98
    mcopy -i install.dsk win98/dboxmpi.drv  ::win98/dboxmpi.drv
    mcopy -i install.dsk win95/dboxmpi.inf  ::win98/dboxmpi.inf

    mmd   -i install.dsk                    ::winme
    mcopy -i install.dsk winme/dboxmpi.drv  ::winme/dboxmpi.drv
    mcopy -i install.dsk win95/dboxmpi.inf  ::winme/dboxmpi.inf
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    true
fi

