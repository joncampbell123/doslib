#!/bin/bash
make -C ../../../../hw/vga png2vrl || exit 1

../../../../hw/vga/png2vrl -i cr52pal.png  -o cr52pal.vrl  -p cr52pal.pal  || exit 1

for png in cr52r{n,l}??.png; do
    vrl=`echo "$png" | sed -e 's/\.png$/.vrl/'`
    if [[ "$vrl" == "$png" ]]; then exit 1; fi
    pal=`echo "$png" | sed -e 's/\.png$/.pal/'`
    if [[ "$pal" == "$png" ]]; then exit 1; fi
    ../../../../hw/vga/png2vrl -i "$png" -o "$vrl" -p "$pal" || exit 1
done

