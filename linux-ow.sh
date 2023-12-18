#!/bin/bash

# allow the user to override WATCOM variable, auto-set otherwise
if [ -z "$WATCOM" ]; then
    export WATCOM=/usr/watcom

    # Jonathan has started developing with OpenWatcom 2.0 branch. Use it!
    if [ -d "/usr/watcom-2.0/binl" ]; then
        export WATCOM=/usr/watcom-2.0
    fi

    # TESTING: If Jon's custom branch of Open Watcom 2.0 is present, use it--this is vital for testing!
    if [ -d "/usr/src/ow/open-watcom-v2/rel/binl" ]; then
        export WATCOM=/usr/src/ow/open-watcom-v2/rel
    fi
    if [ -d "$HOME/src/open-watcom-v2/rel/binl" ]; then
        export WATCOM=$HOME/src/open-watcom-v2/rel
    #elif [ -d "/usr/src/open-watcom-v2-upstream/rel/binl" ]; then
    #    export WATCOM=/usr/src/open-watcom-v2-upstream/rel
    elif [ -d "/usr/src/open-watcom-v2/rel/binl" ]; then
        export WATCOM=/usr/src/open-watcom-v2/rel
    fi

    export PATH=$WATCOM/binl:$WATCOM/binw:$PATH
    export EDPATH=$WATCOM/eddat
    export INCLUDE=$WATCOM/lh
fi

export HPS=/
# PROJTOP: top directory of project we are building
export PROJTOP=`pwd`
# TOP: top directory of entire project. set buildall.sh or make.sh
if [ x"$TOP" == x ]; then
    echo WARNING: TOP directory not set
    sleep 1
fi

# how to invoke 32-bit GCC
if [ -z "$GCC32" ]; then
    x=`uname -m`

    p=~/src/gcc-7.1.0-i386-cc-build/bin/i386-elf-gcc
    l=~/src/gcc-7.1.0-i386-cc-build/bin/i386-elf-ld
    if [ -x "$p" ]; then
        GCC32="$p"
        if [ -x "$l" ]; then GCCLD32="$l"; fi
    fi
fi

# how to invoke 32-bit GCC
if [ -z "$GCC32" ]; then
    x=`uname -m`
    if [ "$x" == "x86_64" ]; then
        GCC32="gcc -m32"
    elif [[ "$x" == "i386" || "$x" == "i486" || "$x" == "i586" || "$x" == "i686" ]]; then
        GCC32=gcc
    fi
fi

# how to invoke 32-bit GCC
if [ -z "$GCCLD32" ]; then
    x=`uname -m`
    if [ "$x" == "x86_64" ]; then
        GCCLD32="i386-pc-linux-gnu-ld"
    elif [[ "$x" == "i386" || "$x" == "i486" || "$x" == "i586" || "$x" == "i686" ]]; then
        GCCLD32=ld
    fi
fi

# script should call this when run with "clean" command. if it has it's own build steps to
# clean up then it should do them after running this subroutine.
do_clean() {
    cd "$PROJTOP" || exit 1
    rm -Rfv {dos,d98}*86{t,s,m,c,l,h,f}{,d} win{1,2,3}{0,1}{0,2,3}{s,m,c,l,h,f} win32s{3,4,5,6}{,d} winnt win32 win386 win38631 os2{d,w}{2,3}{l,f}
    rm -fv nul.err tmp.cmd *~
}

# decide which builds to make.
# out: build_list
make_buildlist() {
    build_list=

    if [ x"$also_build_list" != x ]; then
        build_list="$also_build_list"
    fi

    # assume DOS target
    if [[ x"$win386" == x"1" || x"$win38631" == x"1" || x"$win10" == x"1" || x"$win20" == x"1" || x"$win30" == x"1" || x"$win31" == x"1" || x"$win32s" == x"1" || x"$winnt" == x"1" || x"$win32" == x"1" || x"$os216" == x"1" || x"$os232" == x"1" || x"$vxd31" == x"1" ]]; then
        true
    else
        if [[ x"$dos" == x"" && x"$dostiny" == x"" ]]; then dos=1; fi
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

    # tiny memory model support
    if [ x"$dostiny" == x"1" ]; then
        if [ x"$build_everything" == x"1" ]; then
            build_list="$build_list dos86t dos286t"
        else
            build_list="$build_list dos86t"
        fi
    fi

    # huge memory model support
    if [ x"$doshuge" == x"1" ]; then
        if [ x"$build_everything" == x"1" ]; then
            build_list="$build_list dos86h dos286h"
        else
            build_list="$build_list dos86h"
        fi
    fi

    # PC-98 target
    if [ x"$dospc98" == x"1" ]; then
        if [ x"$dos" == x"1" ]; then
            # NTS: dos86c/m/l/s and dos286... -> real mode 16-bit code
            #      dos386f and so on -> protected mode 32-bit code
            if [ x"$build_everything" == x"1" ]; then
                build_list="$build_list d9886c d9886l d9886m d9886s d98286c d98286l d98286m d98286s d98386f d98486f d98586f d98686f"
            else
                build_list="$build_list d9886c d9886l d9886m d9886s d98386f"
            fi
        fi

        # tiny memory model support
        if [ x"$dostiny" == x"1" ]; then
            if [ x"$build_everything" == x"1" ]; then
                build_list="$build_list d9886t d98286t"
            else
                build_list="$build_list d9886t"
            fi
        fi

        # huge memory model support
        if [ x"$doshuge" == x"1" ]; then
            if [ x"$build_everything" == x"1" ]; then
                build_list="$build_list d9886h d98286h"
            else
                build_list="$build_list d9886h"
            fi
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
    if [ x"$win10" == x"1" ]; then
        # NTS: "win100" = Windows 1.0/8086 target
        # NTS: "win102" = Windows 1.0/286 target
        #      "win103" = Windows 1.0/386 target
        if [ x"$build_everything" == x"1" ]; then
            build_list="$build_list win100l win100c win102l win102c win103l win103c"
        else
            build_list="$build_list win100l win102l"
        fi
    fi

    # NEW: Some portions of this tree produce Win16/Win386 output. Since Windows 3.1/9x/ME sit atop DOS anyway...
    # NTS: Watcom C doesn't work properly emitting "medium/small" memory model output for Windows 3.1. Do not use it!
    #      To demonstate how messed up it is, try using even standard C runtime functions--it will still mismatch near/far types
    if [ x"$win20" == x"1" ]; then
        # NTS: "win200" = Windows 2.0/8086 target
        # NTS: "win202" = Windows 2.0/286 target
        #      "win203" = Windows 2.0/386 target
        if [ x"$build_everything" == x"1" ]; then
            build_list="$build_list win200l win200c win202l win202c win203l win203c"
        else
            build_list="$build_list win200l win202l"
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

    # Windows 3.1 VXD output
    if [ x"$vxd31" == x"1" ]; then
        build_list="$build_list vxd31"
    fi

    if [ x"$allow_build_list" != x ]; then
        build_list="$allow_build_list"
    fi
    if [ x"$parent_build_list" != x ]; then
        build_list="$parent_build_list"
    fi

    export $build_list
}

begin_bat() {
    true
}

end_bat() {
    true
}

bat_wmake() {
    true
}

# in: $1 = what to build
#     $2 = target
do_wmake() {
    _x1="$1"; shift
    _x2="$1"; shift
    cd "$PROJTOP" || return 1
    mkdir -p "$_x1" || return 1
    opts=( )

    # allow "DEBUG=1 ./buildall.sh"
    if [ -n "$DEBUG" ]; then
        opts+=("DEBUG=1")
    fi

    if [ -n "$GCC32" ]; then
        opts+=("GCC32=$GCC32")
    fi
    if [ -n "$GCCLD32" ]; then
        opts+=("GCCLD32=$GCCLD32")
    fi

    wmake -h -e -f "$TOP/mak/$_x1.mak" "${opts[@]}" "TO_BUILD=$_x1" HPS=/ $_x2 REL=$rel $* || return 1
    return 0
}

# $1: name/path of the disk image to create
make_msdos_data_disk() {
    dd if=/dev/zero "of=$1" bs=512 count=2880 || return 0
    mkfs.vfat -n TEST "$1" || return 0
    return 0
}

