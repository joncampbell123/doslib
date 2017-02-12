#!/bin/bash
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
elif [ -d "/usr/src/open-watcom-v2/rel/binl" ]; then
    export WATCOM=/usr/src/open-watcom-v2/rel
fi

export PATH=$WATCOM/binl:$WATCOM/binw:$PATH

cat >tmp.cmd <<_EOF
option quiet
file dboxvxd.obj
option map=dboxvxd.map
system win_vxd
option nocaseexact
segment _LTEXT PRELOAD NONDISCARDABLE
segment _LDATA PRELOAD NONDISCARDABLE
segment _ITEXT DISCARDABLE
segment _IDATA DISCARDABLE
segment _TEXT  NONDISCARDABLE
segment _DATA  NONDISCARDABLE
option nodefaultlibs
option modname=DBOXMPI
option description 'DOSBox-X Mouse Pointer Integration driver for Windows 95'
export DBOXMPI_DDB.1
name dboxmpi.386
_EOF
/usr/src/open-watcom-v2/rel/binl/wlink @tmp.cmd || exit 1
cp -v dboxmpi.386 dboxmpi.386.watcom

