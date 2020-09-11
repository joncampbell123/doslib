#!/bin/bash
make -C ../../../../hw/vga png2vrl || exit 1

../../../../hw/vga/png2vrl -i gmch1.png -o gmch1.vrl -p gmch1.pal || exit 1
../../../../hw/vga/png2vrl -i gmch2.png -o gmch2.vrl -p gmch2.pal || exit 1
../../../../hw/vga/png2vrl -i gmch3.png -o gmch3.vrl -p gmch3.pal || exit 1
../../../../hw/vga/png2vrl -i gmch4.png -o gmch4.vrl -p gmch4.pal || exit 1

# NTS: Shares palette from gmch1.png!
../../../../hw/vga/png2vrl -i gmchm1.png -o gmchm1.vrl -p gmchm1.pal || exit 1
../../../../hw/vga/png2vrl -i gmchm2.png -o gmchm2.vrl -p gmchm2.pal || exit 1

