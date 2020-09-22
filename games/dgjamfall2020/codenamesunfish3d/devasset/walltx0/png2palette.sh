#!/bin/bash
#
# FIXME: ImageMagick can map to a color palette but the PNG encoder rearranges the palette to whatever whim it fancies.
#        Telling the PNG encoder to preserve the color palette makes it forget there is a color palette (what?).
#        We're going to just have to write something to take the PNGs and rearrange the palette back into an order that
#        matches the damn palette we wanted in common across the PNGs in the first place. >:(
#mkdir __DONE_PALUNORD__
convert "$1" -channel A -threshold 50% -channel RGB -ordered-dither o8x8,32,64,32 -map palette.png -define png:compression-level=9 -define png:compression-strategy=0 "png8:$1.palunord.png" || exit 1
#mv -vn "$1" __DONE_PALUNORD__/
