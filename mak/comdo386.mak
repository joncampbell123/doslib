# do not run directly, use make.sh

CFLAGS_1 =

CFLAGS_OPT=-oilrb

# favor execution time
CFLAGS_OPT=$(CFLAGS_OPT) -ot

# inline math functions
CFLAGS_OPT=$(CFLAGS_OPT) -om

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
CPULEV0 = 3
!endif
!ifndef CPULEV2
CPULEV2 = 3
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
TARGET86 = 386
!endif

# PC-98 support
!ifdef PC98
CFLAGS_1 += -dTARGET_PC98=1
!endif

# Watcom does not have -fp4 because there's really nothing new to the 486 FPU to code home about
!ifndef CPULEV3F
CPULEV3F=$(CPULEV3)
!ifeq CPULEV3F 4
CPULEV3F=3
!endif
!endif

!ifndef CPULEV4F
CPULEV4F=$(CPULEV4)
!ifeq CPULEV4F 4
CPULEV4F=3
!endif
!endif

# Include DOS headers.
!if $(__VERSION__) < 1300
WATCOM_INCLUDE=-i"$(%WATCOM)/h"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h"
!endif

EXEEXT=exe
TARGET_MSDOS = 32
!ifdef PC98
SUBDIR   = d98$(TARGET86)$(MMODE)
!else
SUBDIR   = dos$(TARGET86)$(MMODE)
!endif
CC       = *wcc386
CXX      = *wpp386
LINKER   = *wcl386
LDFLAGS  = # -ldos32a
WLINK_SYSTEM = dos4g
WLINK_CON_SYSTEM = dos4g
CFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
CFLAGS386= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos $(CFLAGS_OPT) -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
AFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
NASMFLAGS= -DTARGET_MSDOS=32 -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE32 -I$(REL)/asminc/

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

