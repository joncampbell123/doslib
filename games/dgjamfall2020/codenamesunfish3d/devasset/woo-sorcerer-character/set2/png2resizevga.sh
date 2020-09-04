#!/bin/bash
#
# Fit this 320x360 image into a 160x180 box. Furthermore 320x200 VGA has a non-square aspect ratio, so
# fit it into a 160 x ((180*200)/240) = 160x150 box.
mkdir __DONE_RESIZE__
convert "$1" -resize '160x150!' "$1.rsz.png" || exit 1
mv -v "$1" __DONE_RESIZE__/
