#!/bin/bash

if [ "$1" == "clean" ]; then
    make clean
    exit 0
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
    make
fi

