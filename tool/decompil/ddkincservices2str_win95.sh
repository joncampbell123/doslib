#!/bin/bash
#
# Example script to take VMM.INC and dump all VMM_Service enumerations (Windows 95 DDK)
file=~/Downloads/x/ddk95/Inc32/VMM.INC
grep -R "VMM_Service" $file | sed -e "s/VMM_Service */VMM_Service"$'\t'/ | cut -d $'\t' -f 2 | cut -d ',' -f 1 | sed -e 's/\x0D//g' | while read X; do echo "\"$X\","; done

