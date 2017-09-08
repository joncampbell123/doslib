#!/bin/bash
if [[ !( -f Makefile ) ]]; then
    mkdir -p linux-host || exit 1
    dst="`pwd`/linux-host"
    ./configure "--prefix=$dst" --enable-static --disable-shared || exit 1
fi
if [[ !( -f linux-host/lib/libmspack.a ) ]]; then
    make -j5 || exit 1
    make install || exit 1
fi

