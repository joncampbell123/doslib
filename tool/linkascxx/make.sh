#!/bin/bash
if [ "$1" == "clean" ]; then
    rm -Rfv linux-host
    exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
	true
fi

