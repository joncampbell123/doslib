#!/bin/bash

if [ "$1" == "clean" ]; then
    rm -fv *.{obj,sym,map,exe}
else
    echo "Compiling `pwd`"
    if [ "$SOURCES" != "" ]; then
        # now carry it out
        for x in $SOURCES; do
            echo "    cc: $WCC $SOURCES $WCC_OPTS"
            $WCC $SOURCES $WCC_OPTS || exit 1
        done

        echo "    link: $WLINK"
        $WLINK || exit 1
    fi
fi

