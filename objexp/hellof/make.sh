#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh
if [ "$1" == "clean" ]; then
    rm -Rfv dos86s
    rm -fv *.obj *.lib *.exe *.com *.bin drv.map win95.dsk drva.asm
	exit 0
elif [ "$1" == "disk" ]; then
	# bootable win95 rescue disk with test program
	gunzip -c -d win95.dsk.gz >win95.dsk
    mcopy -i win95.dsk dos86s/drv.exe ::drv.exe
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

