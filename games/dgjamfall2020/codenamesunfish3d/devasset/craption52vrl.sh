#!/bin/bash
make -C ../../../../hw/vga png2vrl || exit 1
make -C ../tool pngmatchpal || exit 1

../../../../hw/vga/png2vrl -i cr52pal.png  -o cr52pal.vrl  -p cr52pal.pal  || exit 1

for png in cr52r{n,l}??.png cr52bt??.png; do
    # *sigh* it would be so nice if I could convert to indexed in GIMP and actually have a palette that matches exactly what I asked it to convert to >:(
    tmppng="$png.palmatch.png"
    ../tool/pngmatchpal -i "$png" -o "$tmppng" -p cr52pal.png
    # convert to VRL and PAL
    vrl=`echo "$png" | sed -e 's/\.png$/.vrl/'`
    if [[ "$vrl" == "$png" ]]; then exit 1; fi
    pal=`echo "$png" | sed -e 's/\.png$/.pal/'`
    if [[ "$pal" == "$png" ]]; then exit 1; fi
    ../../../../hw/vga/png2vrl -i "$tmppng" -o "$vrl" -p "$pal" || exit 1
done

