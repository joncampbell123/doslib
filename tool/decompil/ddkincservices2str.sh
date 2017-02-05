#!/bin/bash
#
# Example script to take VMM.INC and dump all VMM_Service enumerations
file=/mnt/main/emu/win31ddk/DDK/386/include/pageswap.inc
file=~/Downloads/x/ddk95/Inc32/PAGESWAP.INC
count=0
grep -R "PageSwap_Service" $file | cut -d $'\t' -f 2 | cut -d ',' -f 1 | sed -e 's/\x0D//g' | while read X; do
    counth=`printf 0x%04X $count`;
    echo "    \"$X\", /* $counth */";

    count=$(($count+1))
done

