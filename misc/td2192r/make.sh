#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh
if [ "$1" == "clean" ]; then
	rm -fv *.obj *.lib *.exe *.com
	exit 0
else
	wmake -f watcom_c.mak
fi

cat >MAKE.BAT <<_EOF
@echo off

rem shut up DOS4G/W
set DOS4G=quiet

if "%1" == "clean" call clean.bat
if "%1" == "clean" goto end
wmake -f watcom_c.mak
:end
_EOF

cat >CLEAN.BAT <<_EOF
@echo off

del *.obj
del *.exe
del *.lib
del *.com
del foo.gz
_EOF

