#!/usr/bin/bash
rel=../..
if [ x"$TOP" == x ]; then TOP=`pwd`/$rel; fi
. $rel/linux-ow.sh

# FIXME: Only because DOSBox + MSVC++ keep saving filenames in caps
#mv -vn RESOURCE.H resource.h
#mv -vn HELLO.RC hello.rc
#mv -vn HELLO.ICO hello.ico

dos=1 # MS-DOS
# TODO: I think it might be possible for a DOS app to poke into VMM space too, especially 32-bit
# TODO: I think it's possible for a Win16 app to poke into VMM space too
win30=1 # Windows 3.0
win31=1 # Windows 3.1
winnt=1 # Windows NT
win32=1 # Windows 9x/NT/XP/Vista/etc.
win32s=1 # Windows 3.1 + Win32s
#win386=1 # Windows 3.1 + Watcom win386        <- FIXME: Ok great, it produces some proprietary EXE with an 'MQ' signature, how the fuck do I run it?

if [ "$1" == "clean" ]; then
	do_clean
	rm -f *.aps # Damn you Visual C++ AppStudio
	rm -fv test.dsk v86kern.map
	exit 0
fi

if [ "$1" == "disk" ]; then
	# boot data disk
	make_msdos_data_disk test.dsk || exit 1
#	mcopy -i test.dsk win303l/hello4.exe ::4e386_30.exe
fi

if [[ "$1" == "build" || "$1" == "" ]]; then
	make_buildlist
	begin_bat

	what=all
	if [ x"$2" != x ]; then what="$2"; fi
	if [ x"$3" != x ]; then build_list="$3"; fi

	for name in $build_list; do
		do_wmake $name "$what" || exit 1
		bat_wmake $name "$what" || exit 1
	done

	end_bat
fi

