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
WATCOM_INCLUDE=-i"$(%WATCOM)/h;$(%WATCOM)/h/nt;$(%WATCOM)/h/nt/directx;$(%WATCOM)/h/nt/ddk"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h;$(%WATCOM)/h/nt;$(%WATCOM)/h/nt/directx;$(%WATCOM)/h/nt/ddk"
!endif

EXEEXT=exe
TARGET_MSDOS = 32
TARGET_WINDOWS = 40
SUBDIR   = win32
RC       = *wrc
CC       = *wcc386
CXX      = *wpp386
LINKER   = *wcl386
LDFLAGS  = # -ldos32a
WLINK_SYSTEM = nt_win
WLINK_DLL_SYSTEM = nt_dll
WLINK_CON_SYSTEM = nt

RCFLAGS  = -q -r -bt=nt $(WATCOM_INCLUDE)
CFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bg
CFLAGS386= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bg
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bg
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686= -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bg
AFLAGS   = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -bg
NASMFLAGS= -DTARGET_MSDOS=32 -DTARGET86=$(TARGET86) -DTARGET_WINDOWS=$(TARGET_WINDOWS) -DMMODE=$(MMODE) -Dsegment_use=USE32 -I$(REL)/asminc/

RCFLAGS_CON = -q -r -bt=nt $(WATCOM_INCLUDE)
CFLAGS_CON = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bc
CFLAGS386_CON = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bc
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_CON = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bc
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_CON = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bc
AFLAGS_CON = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -bc
NASMFLAGS_CON = -DTARGET_MSDOS=32 -DTARGET86=$(TARGET86) -DTARGET_WINDOWS=$(TARGET_WINDOWS) -DMMODE=$(MMODE) -Dsegment_use=USE32 -I$(REL)/asminc/

RCFLAGS_DLL = -q -r -bt=nt $(WATCOM_INCLUDE)
CFLAGS_DLL = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bd
CFLAGS386_DLL = -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bd
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_DLL = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bd
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_DLL = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) $(CFLAGS_OPT) -bt=nt -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -bd
AFLAGS_DLL = -e=2 -zq -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=nt -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=32 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -bd
NASMFLAGS_DLL = -DTARGET_MSDOS=32 -DTARGET86=$(TARGET86) -DTARGET_WINDOWS=$(TARGET_WINDOWS) -DMMODE=$(MMODE) -Dsegment_use=USE32 -I$(REL)/asminc/

!include "$(REL)/mak/bcommon.mak"
!include "common.mak"
!include "$(REL)/mak/dcommon.mak"

