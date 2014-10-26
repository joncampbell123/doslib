#!/usr/bin/bash
export WATCOM=/usr/watcom

# TESTING: If Jon's custom branch of Open Watcom 1.9 is present, use it--this is vital for testing!
if [ -d "/usr/src/openwatcom-1.9/rel2/binl" ]; then
	export WATCOM=/usr/src/openwatcom-1.9/rel2
fi

# Jonathan has started developing with OpenWatcom 2.0 branch. Use it!
if [ -d "/usr/watcom-2.0/binl" ]; then
	export WATCOM=/usr/watcom-2.0
fi

echo "Using: $WATCOM"

export EDPATH=$WATCOM/eddat
export PATH=$WATCOM/binl:$WATCOM/binw:$PATH
export "INCLUDE=$WATCOM/h/nt;$WATCOM/h/nt/directx;$WATCOM/h/nt/ddk;$WATCOM/h"
export HPS=/
# PROJTOP: top directory of project we are building
export PROJTOP=`pwd`
# TOP: top directory of entire project. set buildall.sh or make.sh
if [ x"$TOP" == x ]; then
	echo WARNING: TOP directory not set
	sleep 1
fi

# script should call this when run with "clean" command. if it has it's own build steps to
# clean up then it should do them after running this subroutine.
do_clean() {
	cd "$PROJTOP" || exit 1
	rm -Rfv dos*86{s,m,c,l,h,f}{,d} win3{0,1}{0,2,3}{s,m,c,l,h,f} win32s{3,4,5,6}{,d} winnt win32 win386 win38631 os2{d,w}{2,3}{l,f}
	rm -fv nul.err tmp.cmd *~
}

# decide which builds to make.
# out: build_list
make_buildlist() {
	build_list=

	# assume DOS target
	if [[ x"$win386" == x"1" || x"$win38631" == x"1" || x"$win30" == x"1" || x"$win31" == x"1" || x"$win32s" == x"1" || x"$winnt" == x"1" || x"$win32" == x"1" || x"$os216" == x"1" || x"$os232" == x"1" ]]; then
		true
	else
		if [[ x"$dos" == x"" ]]; then dos=1; fi
	fi

	# Most code here is DOS oriented
	if [ x"$dos" == x"1" ]; then
		# NTS: dos86c/m/l/s and dos286... -> real mode 16-bit code
		#      dos386f and so on -> protected mode 32-bit code
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list dos86c dos86l dos86m dos86s dos286c dos286l dos286m dos286s dos386f dos486f dos586f dos686f"
		else
			build_list="$build_list dos86c dos86l dos86m dos86s dos386f"
		fi
	fi

	# some parts can compile as 16-bit OS/2 application code (command line).
	# Note that OS/2 16-bit means "OS/2 16-bit code for OS/2 1.x or higher".
	if [ x"$os216" == x"1" ]; then
		# NTS: os2w2l = OS/2 16-bit large memory model, command line program, 286 or higher
		#      os2w3l = OS/2 16-bit large memory model, command line program, 386 or higher
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list os2w2l os2w3l"
		else
			build_list="$build_list os2w2l"
		fi
	fi

	# some parts can compile as 32-bit OS/2 application code (command line)
	# Note that OS/2 32-bit means "OS/2 32-bit code for OS/2 2.x or higher"
	if [ x"$os232" == x"1" ]; then
		# NTS: os2d3f = OS/2 32-bit large memory model, command line program, 386 or higher
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list os2d3f"
		else
			build_list="$build_list os2d3f"
		fi
	fi

	# NEW: Some portions of this tree produce Win16/Win386 output. Since Windows 3.1/9x/ME sit atop DOS anyway...
	# NTS: Watcom C doesn't work properly emitting "medium/small" memory model output for Windows 3.1. Do not use it!
	#      To demonstate how messed up it is, try using even standard C runtime functions--it will still mismatch near/far types
	if [ x"$win30" == x"1" ]; then
		# NTS: "win300" = Windows 3.0/8086 target
		# NTS: "win302" = Windows 3.0/286 target
		#      "win303" = Windows 3.0/386 target
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list win300l win300c win302l win302c win303l win303c"
		else
			build_list="$build_list win302l"
		fi
	fi

	# NEW: Some portions of this tree produce Win16/Win386 output. Since Windows 3.1/9x/ME sit atop DOS anyway...
	# NTS: Watcom C doesn't work properly emitting "medium/small" memory model output for Windows 3.1. Do not use it!
	#      To demonstate how messed up it is, try using even standard C runtime functions--it will still mismatch near/far types
	if [ x"$win31" == x"1" ]; then
		# NTS: "win312" = Windows 3.1/286 target
		#      "win313" = Windows 3.1/386 target
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list win312l win312c win313l win313c"
		else
			build_list="$build_list win313l"
		fi
	fi

	# NEW: Some portions of this tree produce Windows 3.1 Win32s compatible output
	if [ x"$win32s" == x"1" ]; then
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list win32s3 win32s4 win32s5 win32s6"
		else
			build_list="$build_list win32s3"
		fi
	fi

	# NEW: Some portions of this tree produce Win32 (Windows NT) output
	if [ x"$winnt" == x"1" ]; then
		build_list="$build_list winnt"
	fi

	# NEW: Some portions of this tree produce Win32 (Windows 9x & NT) output
	if [ x"$win32" == x"1" ]; then
		build_list="$build_list win32"
	fi

	# NEW: Some portions of this tree produce Windows 3.0 win386 output
	if [ x"$win386" == x"1" ]; then
		if [ x"$build_everything" == x"1" ]; then
			build_list="$build_list win386 win38631"
		else
			build_list="$build_list win386"
		fi
	fi

	# Windows 3.1 win386 output
	if [ x"$win38631" == x"1" ]; then
		build_list="$build_list win38631"
	fi

	if [ x"$allow_build_list" != x ]; then
		echo "Allow list: $allow_build_list"
		build_list="$allow_build_list"
	fi
	if [ x"$parent_build_list" != x ]; then
		echo "Parent list: $parent_build_list"
		build_list="$parent_build_list"
	fi

	export $build_list
}

# begin generating BAT files for equivalent build functions under MS-DOS
# in: PROJTOP = Directory of sub-project we are building
# out: CLEAN_BAT_PATH = path of CLEAN.BAT
#      MAKE_BAT_PATH = path of MAKE.BAT
begin_bat() {
	CLEAN_BAT_PATH=$PROJTOP/CLEAN.BAT
	cat >$CLEAN_BAT_PATH <<_EOF
@echo off

_EOF

	MAKE_BAT_PATH=$PROJTOP/MAKE.BAT
	cat >$MAKE_BAT_PATH <<_EOF
@echo off
rem ----- Auto-generated by make.sh and Linux make system do not modify

rem ----- shut up DOS4G/W
set DOS4G=quiet

if "%1" == "clean" call clean.bat
if "%1" == "clean" goto end

_EOF
}

# in: $1 = what to build
#     $2 = target
do_wmake() {
	_x1="$1"; shift
	_x2="$1"; shift
	cd "$PROJTOP" || return 1
	mkdir -p "$_x1" || return 1
	wmake -f "$TOP/mak/$_x1.mak" "TO_BUILD=$_x1" HPS=/ $_x2 REL=$rel $* || return 1
	return 0
}

# in: $1 what to build
#     $2 target
#     CLEAN_BAT_PATH path of CLEAN.BAT
#     MAKE_BAT_PATH path of MAKE.BAT
bat_wmake() {
	_x1="$1"; shift
	_x2="$1"; shift
	dos_rel=`echo "$rel" | sed -e 's/\\//\\\\/g'`
	cat >>$MAKE_BAT_PATH <<_EOF
mkdir $_x1
wmake -f $dos_rel\\mak\\$_x1.mak HPS=\\ $_x2 REL=$dos_rel $*

_EOF
	cat >>$CLEAN_BAT_PATH <<_EOF
deltree /Y $1
del /s /q $_x1\\*.*
_EOF
}

end_bat() {
	cat >>$MAKE_BAT_PATH <<_EOF

:end
_EOF

	cat >>$CLEAN_BAT_PATH <<_EOF
del *.obj
del *.exe
del *.lib
del *.com
del foo.gz
_EOF
}

# $1: name/path of the disk image to create
make_msdos_data_disk() {
	dd if=/dev/zero of=test.dsk bs=512 count=2880 || return 0
	mkfs.vfat -n TEST test.dsk || return 0
	return 0
}

