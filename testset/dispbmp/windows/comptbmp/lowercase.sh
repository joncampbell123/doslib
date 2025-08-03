#!/bin/bash
for file in *.{ico,ICO,rc,RC} RESOURCE.H resource.h; do
	if [ -f "$file" ]; then
		lc=`echo "$file" | tr '[:upper:]' '[:lower:]'`
		if [ "$file" == "$lc" ]; then
			true
		else
			mv -v "$file" "$lc" || exit 1
		fi
	fi
done

