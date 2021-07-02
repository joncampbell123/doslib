#!/bin/bash
# $1 = input file
# $2 = number of colors (color palette size)
# $3 = output PNG file
convert "$1" +dither -colors "$2" "$3.pal.png"
