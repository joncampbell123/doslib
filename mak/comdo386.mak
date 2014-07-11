# do not run directly, use make.sh

CFLAGS_1 =
!ifndef DEBUG
DEBUG = -d0
DSUFFIX =
!else
DSUFFIX = d
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

TARGET_MSDOS = 32
SUBDIR   = dos$(TARGET86)$(MMODE)$(DSUFFIX)
CC       = wcc386
LINKER   = wcl386
LDFLAGS  = # -ldos32a
WLINK_SYSTEM = dos4g
WLINK_CON_SYSTEM = dos4g
CFLAGS   = -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) -bt=dos -oilrtfm -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
CFLAGS386= -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) -bt=dos -oilrtfm -wx -fp$(CPULEV3F) -$(CPULEV3)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586= -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) -bt=dos -oilrtfm -wx -fp$(CPULEV5) -$(CPULEV5)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686= -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) -bt=dos -oilrtfm -wx -fp$(CPULEV6) -$(CPULEV6)r -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
AFLAGS   = -e=2 -zq -m$(MMODE) $(DEBUG) $(CFLAGS_1) -bt=dos -wx -fp$(CPULEV3F) -$(CPULEV3) -dTARGET_MSDOS=32 -dMSDOS=1 -dTARGET86=$(TARGET86) -DMMODE=$(MMODE) -q
NASMFLAGS= -DTARGET_MSDOS=32 -DMSDOS=1 -DTARGET86=$(TARGET86) -DMMODE=$(MMODE)

# NTS: MS-DOS is console based, no difference
CFLAGS_CON = $(CFLAGS)
CFLAGS386_CON = $(CFLAGS386)
# a 586 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_586_CON = $(CFLAGS386_TO_586)
# a 686 version of the build flags, so some OBJ files can target Pentium or higher instructions
CFLAGS386_TO_686_CON = $(CFLAGS386_TO_686)
AFLAGS_CON = $(AFLAGS)
NASMFLAGS_CON = $(NASMFLAGS)

!include "$(REL)$(HPS)mak$(HPS)bcommon.mak"
!include "common.mak"
!include "$(REL)$(HPS)mak$(HPS)dcommon.mak"

