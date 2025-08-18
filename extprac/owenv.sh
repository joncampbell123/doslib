#!/bin/bash
#
# How to use:
#
# . owenv.sh
#
# Then you can use the Watcom compiler

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
    elif [ -d "/usr/src/open-watcom-v2/rel/binl" ]; then
        export WATCOM=/usr/src/open-watcom-v2/rel
    fi
    if [ -d "/usr/src/open-watcom-x/rel/binl" ]; then
        export WATCOM=/usr/src/open-watcom-x/rel
    fi

    export EDPATH=$WATCOM/eddat
    export PATH=$WATCOM/binl:$WATCOM/binw:$PATH
    export INCLUDE=$WATCOM/lh
fi

echo "Using: $WATCOM"

export HPS=/

