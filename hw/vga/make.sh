#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1 # MS-DOS
win31=1 # Windows 3.1
win32s=1 # Windows 3.1 + Win32s
win32=1 # Windows 9x/NT/XP/Vista/etc.

if [ "$1" == "clean" ]; then
	do_clean
	rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
	exit 0
fi

if [ "$1" == "disk" ]; then
	make_msdos_data_disk test.dsk || exit 1
	mcopy -i test.dsk dos86s/test.exe ::test86.exe
	mcopy -i test.dsk dos86l/test.exe ::test86l.exe
	mcopy -i test.dsk dos386f/test.exe ::test386.exe
	mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
	mcopy -i test.dsk win313l/test.exe ::testw31.exe
	mcopy -i test.dsk win32s3/test.exe ::testw32s.exe
	mcopy -i test.dsk win32/test.exe ::testw32.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
	make_buildlist
	begin_bat

	what=all
	if [ x"$2" != x ]; then what="$2"; fi

	for name in $build_list; do
		do_wmake $name "$what" || exit 1
		bat_wmake $name "$what" || exit 1
	done

	end_bat
fi

