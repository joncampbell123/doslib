# do not run directly, use make.sh

CFLAGS_1 =
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

# Include the 1.x headers. I looked at their os2 headers and they seem optimized for 32-bit
!if $(__VERSION__) < 1300
WATCOM_INCLUDE=-i"$(%WATCOM)/h;$(%WATCOM)/h/os21x"
.BEFORE
	set INCLUDE=
!else
WATCOM_INCLUDE=-x -i"$(%WATCOM)/h;$(%WATCOM)/h/os21x"
!endif

# NOTE TO SELF: If you compile using naive flags, your code will work under Windows 3.1, but will crash horribly under Windows 3.0.
#               Wanna know why? Because apparently Windows 3.0 doesn't maintain SS == DS, which Watcom assumes. So you always need
#               to specify the -zu and -zw switches. Even for Windows 3.1.

EXEEXT=exe
TARGET_MSDOS = 16
TARGET_OS2 = 10
SUBDIR   = os2w$(TARGET86_1DIGIT)$(MMODE)
RC       = *wrc
CC       = *wcc
CXX      = *wpp
LINKER   = *wcl
WLINK_SYSTEM = os2_pm
WLINK_CON_SYSTEM = os2
WLINK_DLL_SYSTEM = os2_dll

# GUI versions
RCFLAGS  = -q -r -31 -bt=os2 -D__OS2_PM__ $(WATCOM_INCLUDE)
CFLAGS   = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -D__OS2_PM__ -oilrtfm -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -D__OS2_PM__ -oilrtfm -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386_TO_586= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -D__OS2_PM__ -oilrtfm -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386_TO_686= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -D__OS2_PM__ -oilrtfm -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
AFLAGS   = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -D__OS2_PM__ -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -D_OS2_16_ -bg
NASMFLAGS= -DTARGET_MSDOS=16 -DTARGET_OS2=$(TARGET_OS2) -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/
WLINK_FLAGS = op start=_cstart_ 

# console versions
RCFLAGS_CON  = -q -r -31 -bt=os2 $(WATCOM_INCLUDE)
CFLAGS_CON   = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386_CON= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386_TO_586_CON= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
CFLAGS386_TO_686_CON= -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bg
AFLAGS_CON   = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -D_OS2_16_ -bg
NASMFLAGS_CON= -DTARGET_MSDOS=16 -DTARGET_OS2=$(TARGET_OS2) -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/
WLINK_CON_FLAGS = op start=_cstart_ 

RCFLAGS_DLL = -q -r -31 -bt=os2 $(WATCOM_INCLUDE)
CFLAGS_DLL = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bd
CFLAGS386_DLL = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -$(CPULEV3) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bd
CFLAGS386_TO_586_DLL = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -fp$(CPULEV5) -$(CPULEV5) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bd
CFLAGS386_TO_686_DLL = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -oilrtfm -wx -fp$(CPULEV6) -$(CPULEV6) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q $(WATCOM_INCLUDE) -D_OS2_16_ -bd
AFLAGS_DLL = -e=2 -zq -zu -m$(MMODE) $(CFLAGS_DEBUG) $(CFLAGS_1) -bt=os2 -wx -$(CPULEV0) -dTARGET_MSDOS=16 -dTARGET_OS2=$(TARGET_OS2) -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q -D_OS2_16_ -bd
NASMFLAGS_DLL = -DTARGET_MSDOS=16 -DTARGET_OS2=$(TARGET_OS2) -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE) -Dsegment_use=USE16 -I$(REL)/asminc/

!include "$(REL)/mak/bcommon.mak"
!include "common.mak"
!include "$(REL)/mak/dcommon.mak"

