#!/usr/bin/bash

cat >MAKE.BAT <<_EOF
@echo off
if "%1" == "clean" call clean.bat
if "%1" == "clean" goto end

rem shut up DOS4G/W
set DOS4G=quiet


_EOF

if [ "$SOURCES" != "" ]; then
    for x in $SOURCES; do
        cat >>MAKE.BAT <<_EOF
$WCC $SOURCES $WCC_OPTS
_EOF
    done
fi

cat >>MAKE.BAT <<_EOF
$WLINK

:end
_EOF

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

cat >CLEAN.BAT <<_EOF
@echo off
del *.obj
del *.sym
del *.map
del *.exe
del *.com
del *.lib
_EOF

