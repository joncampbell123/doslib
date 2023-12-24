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

# Encourage Open Watcom to compile all our code into one segment _TEXT for Windows real-mode compatible builds.
# Windows in real mode will obscurely complain about "not enough memory" when attempting to run an EXE with more
# than (about) 5 or 6 segments. Right. Out of memory running a 30KB executable. Sure.
# 
# Protected mode builds on the other hand are free to have many segments.
#
# To keep complex programs sane, we'll just insist on putting everything in one segment anyway.
CFLAGS_1 += -nt=_TEXT

# FIXME
!if $(__VERSION__) < 1300
WATCOM_INCLUDE=-i"$(%WATCOM)/h;$(%WATCOM)/h/win"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h;$(%WATCOM)/h/win"
!endif

# NOTE TO SELF: If you compile using naive flags, your code will work under Windows 3.1, but will crash horribly under Windows 3.0.
#               Wanna know why? Because apparently Windows 3.0 doesn't maintain SS == DS, which Watcom assumes. So you always need
#               to specify the -zu and -zw switches. Even for Windows 3.1

EXEEXT=exe
TARGET_MSDOS = 16
TARGET_WINDOWS = 20
TARGET_WINVER = 0x0200
SUBDIR   = win20$(TARGET86_1DIGIT)$(MMODE)
RC       = *wrc
CC       = *wcc
CXX      = *wpp
LINKER   = *wcl
WLINK_SYSTEM = windows
WLINK_CON_SYSTEM = windows
WLINK_DLL_SYSTEM = windows_dll
RCFLAGS  = -q -r -30 -bt=windows $(WATCOM_INCLUDE) -dNO_DS_SETFONT=1
CFLAGS   = -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bg -dWINVER=$(TARGET_WINVER)
CFLAGS386= -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bg -dWINVER=$(TARGET_WINVER)
CFLAGS386_TO_586= -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bg -dWINVER=$(TARGET_WINVER)
CFLAGS386_TO_686= -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bg -dWINVER=$(TARGET_WINVER)
AFLAGS   = -e=2 -zq -zw -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -D_WINDOWS_16_ -bg -dWINVER=$(TARGET_WINVER)
NASMFLAGS= -DTARGET_MSDOS=16 -DTARGET_WINDOWS=$(TARGET_WINDOWS) -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/ -DWINVER=$(TARGET_WINVER)

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

RCFLAGS_DLL = -q -r -30 -bt=windows $(WATCOM_INCLUDE) -dNO_DS_SETFONT=1
CFLAGS_DLL = -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bd -dWINVER=$(TARGET_WINVER)
CFLAGS386_DLL = -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bd -dWINVER=$(TARGET_WINVER)
CFLAGS386_TO_586_DLL = -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bd -dWINVER=$(TARGET_WINVER)
CFLAGS386_TO_686_DLL = -e=2 -zq -zu -zw -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows $(CFLAGS_OPT) -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_WINDOWS_16_ -bd -dWINVER=$(TARGET_WINVER)
AFLAGS_DLL = -e=2 -zq -zw -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=windows -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_WINDOWS=$(TARGET_WINDOWS) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -D_WINDOWS_16_ -bd -dWINVER=$(TARGET_WINVER)
NASMFLAGS_DLL = -DTARGET_MSDOS=16 -DTARGET_WINDOWS=$(TARGET_WINDOWS) -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/ -DWINVER=$(TARGET_WINVER)

# macro to patch the EXE to the proper version
WIN_NE_SETVER_BUILD = $(REL)$(HPS)tool$(HPS)chgnever.pl 2.0

!include "$(REL)/mak/bcommon.mak"
!include "common.mak"
!include "$(REL)/mak/dcommon.mak"

