# do not run directly, use make.sh

CFLAGS_1 =

CFLAGS_OPT=-oilrb

# favor code size
CFLAGS_OPT=$(CFLAGS_OPT) -os

!ifdef DEBUG
# traceable stack frames
CFLAGS_OPT=$(CFLAGS_OPT) -of
!endif

!ifndef DEBUG
# if not debugging, remove stack frame checks
CFLAGS_1=$(CFLAGS_1) -s
!endif

!ifndef DEBUG
CFLAGS_DEBUG = -d0
!else
CFLAGS_DEBUG = -d9
!endif

!ifndef CPULEV0
CPULEV0 = 0
!endif
!ifndef CPULEV2
CPULEV2 = 2
!endif
!ifndef CPULEV3
CPULEV3 = 3
!endif
!ifndef CPULEV4
CPULEV4 = 4
!endif
!ifndef CPULEV5
CPULEV5 = 5
!endif
!ifndef CPULEV6
CPULEV6 = 6
!endif
!ifndef TARGET86
TARGET86 = 86
!endif

# large and compact model builds must not assume DS != SS.
# some interrupt handlers call subroutines in this code. if not notified that DS != SS
# the subroutines will mis-address parameters and screw things up.
!ifeq MMODE l
ZU_FLAG=-zu
!endif
!ifeq MMODE c
ZU_FLAG=-zu
!endif

# PC-98 support
!ifdef PC98
CFLAGS_1 += -dTARGET_PC98=1
!endif

!ifdef TINYMODE
MMODEC=t
EXEEXT=com
!else
MMODEC=$(MMODE)
EXEEXT=exe
!endif

TARGET_MSDOS = 16
!ifdef PC98
SUBDIR   = d98$(TARGET86)$(MMODEC)
!else
SUBDIR   = dos$(TARGET86)$(MMODEC)
!endif
CC       = *wcc
CXX      = *wpp
LINKER   = *wcl

!ifdef TINYMODE
WLINK_SYSTEM = com
WLINK_CON_SYSTEM = com
!else
WLINK_SYSTEM = dos
WLINK_CON_SYSTEM = dos
!endif

# Include DOS headers.
!if $(__VERSION__) < 1300
WATCOM_INCLUDE=-i"$(%WATCOM)/h"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h"
!endif

CFLAGS   = -e=2 $(ZU_FLAG) -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
CFLAGS386= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
CFLAGS386_TO_586= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
CFLAGS386_TO_686= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
AFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
NASMFLAGS= -DTARGET_MSDOS=16 -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/

!ifdef TINYMODE
AFLAGS += -DTINYMODE=1 -D__TINY__=1
NASMFLAGS += -DTINYMODE=1 -D__TINY__=1
CFLAGS += -zdp -zc -g=DGROUP -nt=_TEXT -nc=CODE -DTINYMODE=1 -D__TINY__=1 -zt=65535
CFLAGS386 += -zdp -zc -g=DGROUP -nt=_TEXT -nc=CODE -DTINYMODE=1 -D__TINY__=1 -zt=65535
CFLAGS386_TO_586 += -zdp -zc -g=DGROUP -nt=_TEXT -nc=CODE -DTINYMODE=1 -D__TINY__=1 -zt=65535
CFLAGS386_TO_686 += -zdp -zc -g=DGROUP -nt=_TEXT -nc=CODE -DTINYMODE=1 -D__TINY__=1 -zt=65535
!endif
!ifdef PC98
NASMFLAGS += -DTARGET_PC98=1
!endif

# NTS: MS-DOS is console based, no difference
CFLAGS_CON = $(CFLAGS)
CFLAGS386_CON = $(CFLAGS386)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_CON = $(CFLAGS386_TO_586)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_CON = $(CFLAGS386_TO_686)
AFLAGS_CON = $(AFLAGS)
NASMFLAGS_CON = $(NASMFLAGS)

!include "$(REL)/mak/bcommon.mak"
!include "common.mak"
!include "$(REL)/mak/dcommon.mak"

