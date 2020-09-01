#!/bin/bash
mkdir -p __DONE__
ffmpeg -i "$1" -an -vcodec png "$1.%03d.png" || exit 1
mv -vn "$1" __DONE__/

