# do not run directly, use make.sh

MMODE=f

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

# ignore default library
CFLAGS_OPT=$(CFLAGS_OPT) -zl

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

!ifeq TARGET86 86
TARGET86_1DIGIT=0
!endif
!ifeq TARGET86 186
TARGET86_1DIGIT=1
!endif
!ifeq TARGET86 286
TARGET86_1DIGIT=2
!endif
!ifeq TARGET86 386
TARGET86_1DIGIT=3
!endif
!ifeq TARGET86 486
TARGET86_1DIGIT=4
!endif
!ifeq TARGET86 586
TARGET86_1DIGIT=5
!endif
!ifeq TARGET86 686
TARGET86_1DIGIT=6
!endif

# FIXME
!if $(__VERSION__) < 1300
WATCOM_INCLUDE=-i"$(%WATCOM)/h"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h"
!endif

EXEEXT=exe
TARGET_MSDOS = 32
TARGET_VXD = 1
SUBDIR   = vxd31
RC       = *wrc
CC       = *wcc386
CXX      = *wpp386
LINKER   = *wcl386
LDFLAGS  = # -ldos32a
WLINK_SYSTEM = win_vxd
WLINK_CON_SYSTEM = win_vxd
WLINK_DLL_SYSTEM = win_vxd
RCFLAGS  = -q -r -31 -bt=windows $(WATCOM_INCLUDE)
CFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=dos -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=$(TARGET_MSDOS) -dMSDOS=1 -dTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
CFLAGS386= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=dos -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=$(TARGET_MSDOS) -dMSDOS=1 -dTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=dos -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=$(TARGET_MSDOS) -dMSDOS=1 -dTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=dos -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=$(TARGET_MSDOS) -dMSDOS=1 -dTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE)
AFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=dos -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=$(TARGET_MSDOS) -dMSDOS=1 -dTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -q
NASMFLAGS= -DTARGET_MSDOS=$(TARGET_MSDOS) -DMSDOS=1 -DTARGET86=$(TARGET86) -dTARGET_VXD=$(TARGET_VXD) -DMMODE=$(MMODE) -Dsegment_use=USE32 -I$(REL)/asminc/

# NTS: Win16 apps do not have a console mode
RCFLAGS_CON = $(RCFLAGS)
CFLAGS_CON = $(CFLAGS)
CFLAGS386_CON = $(CFLAGS386)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_CON = $(CFLAGS386_TO_586)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_CON = $(CFLAGS386_TO_686)
AFLAGS_CON = $(AFLAGS)
NASMFLAGS_CON = $(NASMFLAGS)

RCFLAGS_DLL = $(RCFLAGS)
CFLAGS_DLL = $(CFLAGS)
CFLAGS386_DLL= $(CFLAGS386)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_DLL= $(CFLAGS386_TO_586)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_DLL= $(CFLAGS386_TO_686)
AFLAGS_DLL = $(AFLAGS)
NASMFLAGS_DLL = $(NASMFLAGS)

!include "$(REL)/mak/bcommon.mak"
!include "common.mak"
!include "$(REL)/mak/dcommon.mak"

