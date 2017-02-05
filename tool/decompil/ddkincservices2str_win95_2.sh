#!/bin/bash
#
# Example script to take VMM.INC and dump all VMM_Service enumerations (Windows 95 DDK)
file=~/Downloads/x/ddk95/Inc32/DEBUG.INC
prefix="DEBUG"
grep -R $prefix"_Service" $file | cut -d $'\t' -f 2 | sed -e 's/^ *//' | cut -d ',' -f 1 | sed -e 's/\x0D//g' | while read X; do
    counth=`printf 0x%04X $count`;
    echo "    \"$X\", /* $counth */";

    count=$(($count+1))
done

