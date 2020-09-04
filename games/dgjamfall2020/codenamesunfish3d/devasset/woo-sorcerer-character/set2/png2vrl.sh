#!/bin/bash
#
# Convert the paletted and properly sized PNG to a VRL that can be optimially rendered
# in VGA unchained 256-color mode.
make -C ../../../../../../hw/vga png2vrl || exit 1
../../../../../../hw/vga/png2vrl -i "$1" -o "$1.vrl" -p "$1.pal" || exit 1
