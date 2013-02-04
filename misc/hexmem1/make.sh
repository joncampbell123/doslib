#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

if [ "$1" == "clean" ]; then
	do_clean
	rm -fv test.dsk v86kern.map
	exit 0
fi

if [ "$1" == "disk" ]; then
	# boot data disk
	make_msdos_data_disk test.dsk || exit 1
	mcopy -i test.dsk dos86c/hexmem.exe ::hexm86c.exe
	mcopy -i test.dsk dos86s/hexmem.exe ::hexm86s.exe
	mcopy -i test.dsk dos86m/hexmem.exe ::hexm86m.exe
	mcopy -i test.dsk dos86l/hexmem.exe ::hexm86l.exe
	mcopy -i test.dsk dos386f/hexmem.exe ::hexm386.exe
	mcopy -i test.dsk dos386f/dos4gw.exe ::dos4gw.exe
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

