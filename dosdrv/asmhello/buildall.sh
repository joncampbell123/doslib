#!/bin/bash
# 
# Unlike the DOS batch files, this script is much more powerful.
# In fact, this script BUILDs the DOS batch files.
# 
# NTS: Make sure your editor uses unix LF endings in the file,
#      but keeps the CR LF DOS endings for the DOS batch files.

for i in *; do if [ -d "$i" ]; then
    (cd $i && (
        cp -v -u ../buildall.sh buildall.sh || exit 1

        if [ -x buildall.sh ]; then
            ./buildall.sh $* || exit 1
        fi

        if [ -x make.sh ]; then
            ./make.sh $* || exit 1
        fi)
    ) || exit 1
fi; done

