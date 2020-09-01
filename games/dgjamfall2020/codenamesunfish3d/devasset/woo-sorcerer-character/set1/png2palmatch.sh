#!/bin/bash
#
# Imagemagick uses whatever palette order per image it likes.
# This script runs our internal PNG palette remapper to make them consistent with palette.png.
make -C ../../../tool/pngmatchpal || exit 1
mkdir __DONE_PALORD__
../../../tool/pngmatchpal "$1" "$1.palord.png" || exit 1
mv -v "$1" __DONE_PALORD__/
