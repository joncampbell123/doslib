#!/bin/bash
convert "$1" +dither -colors "$2" "$1.pal.png"
