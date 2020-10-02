#!/bin/bash
make -C ../../../../hw/vga png2vrl || exit 1

../../../../hw/vga/png2vrl -i cr52pal.png  -o cr52pal.vrl  -p cr52pal.pal  || exit 1

