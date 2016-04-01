#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

dos=1
dospc98=1 # MS-DOS PC98
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s

if [ "$1" == "clean" ]; then
	do_clean
	rm -fv test.dsk test2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
	exit 0
fi

if [ "$1" == "disk" ]; then
	make_msdos_data_disk test.dsk || exit 1
#	mcopy -i test.dsk dos86c/playmp3.exe ::pmp386c.exe
#	mcopy -i test.dsk dos86s/playmp3.exe ::pmp386s.exe
#	mcopy -i test.dsk dos86m/playmp3.exe ::pmp386m.exe
#	mcopy -i test.dsk dos86l/playmp3.exe ::pmp386l.exe
#	mcopy -i test.dsk dos286c/playmp3.exe ::pmp3286c.exe
#	mcopy -i test.dsk dos286s/playmp3.exe ::pmp3286s.exe
#	mcopy -i test.dsk dos286m/playmp3.exe ::pmp3286m.exe
#	mcopy -i test.dsk dos286l/playmp3.exe ::pmp3286l.exe
	mcopy -i test.dsk dos386f/playmp3.exe ::pmp3386.exe
	mcopy -i test.dsk dos486f/playmp3.exe ::pmp3486.exe
	mcopy -i test.dsk dos586f/playmp3.exe ::pmp3586.exe
	mcopy -i test.dsk dos686f/playmp3.exe ::pmp3686.exe
	mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
#	allow_build_list="dos386f dos486f dos586f dos686f"
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

