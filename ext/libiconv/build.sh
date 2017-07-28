#!/bin/bash
if [[ !( -f Makefile ) ]]; then
    ./configure --prefix=/tmp/dummy --enable-static --disable-shared || exit 1
fi
make -j5 || exit 1

