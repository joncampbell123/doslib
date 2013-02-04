#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
	do_clean
	rm -fv test.dsk lame2.dsk nul.err tmp.cmd tmp1.cmd tmp2.cmd
	exit 0
fi

if [ "$1" == "disk" ]; then
	make_msdos_data_disk test.dsk || exit 1
#	mcopy -i test.dsk dos86c/lame.exe ::lame86c.exe
#	mcopy -i test.dsk dos86s/lame.exe ::lame86s.exe
#	mcopy -i test.dsk dos86m/lame.exe ::lame86m.exe
#	mcopy -i test.dsk dos86l/lame.exe ::lame86l.exe
#	mcopy -i test.dsk dos286c/lame.exe ::lame286c.exe
#	mcopy -i test.dsk dos286s/lame.exe ::lame286s.exe
#	mcopy -i test.dsk dos286m/lame.exe ::lame286m.exe
#	mcopy -i test.dsk dos286l/lame.exe ::lame286l.exe
	mcopy -i test.dsk dos386f/lame.exe ::lame386.exe
	mcopy -i test.dsk dos486f/lame.exe ::lame486.exe
	mcopy -i test.dsk dos586f/lame.exe ::lame586.exe
	mcopy -i test.dsk dos686f/lame.exe ::lame686.exe
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

