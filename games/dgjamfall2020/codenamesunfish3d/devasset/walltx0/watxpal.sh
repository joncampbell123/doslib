#!/bin/bash
for i in watx????.24bpp.png; do
    ./png2palette.sh "$i" || exit 1
    ./png2palmatch.sh "$i.palunord.png" || exit 1
done
