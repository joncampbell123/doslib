
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..$(HPS).. -s -zq -zl -zu -zw -zc
NOW_BUILDING = WINDRV_DOSBOXPI_WIN9XOW

DISCARDABLE_CFLAGS = -nt=_TEXT -nc=CODE
NONDISCARDABLE_CFLAGS = -nt=_NDTEXT -nc=NDCODE

DBOXMPI_DRV = $(SUBDIR)$(HPS)dboxmpi.drv

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(DISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

.ASM.OBJ:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

$(SUBDIR)$(HPS)inthndlr.obj: inthndlr.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(NONDISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(DBOXMPI_DRV) .symbolic

lib: .symbolic

$(SUBDIR)$(HPS)dboxmpi.res: dboxmpi.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)dboxmpi.res  $[@

!ifdef DBOXMPI_DRV
$(DBOXMPI_DRV): $(HW_DOSBOXID_LIB) $(SUBDIR)$(HPS)dboxmpi.obj $(SUBDIR)$(HPS)dllentry.obj $(SUBDIR)$(HPS)inthndlr.obj $(SUBDIR)$(HPS)dboxmpi.res
	%write tmp.cmd option quiet
	%write tmp.cmd file $(SUBDIR)$(HPS)dllentry.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)dboxmpi.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)inthndlr.obj
	%write tmp.cmd $(HW_DOSBOXID_LIB_DRV_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(DBOXMPI_DRV).map
	%write tmp.cmd option osname='Windows 16-bit'
	%write tmp.cmd libpath %WATCOM%/lib286
	%write tmp.cmd libpath %WATCOM%/lib286/win
	%write tmp.cmd library windows
	%write tmp.cmd option nocaseexact
	%write tmp.cmd option stack=8k, heapsize=1k
	%write tmp.cmd format windows dll
	%write tmp.cmd option resource=$(SUBDIR)$(HPS)dboxmpi.res
	%write tmp.cmd segment TYPE CODE PRELOAD DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED SHARED
	%write tmp.cmd segment _NDTEXT PRELOAD FIXED SHARED
	%write tmp.cmd option nodefaultlibs
	%write tmp.cmd option alignment=16
	%write tmp.cmd option version=4.0  # FIXME: Is Watcom's linker IGNORING THIS?
	%write tmp.cmd option modname=MOUSE
	%write tmp.cmd option description 'DOSBox-X Mouse Pointer Integration driver for Windows 95'
	%write tmp.cmd export Inquire.1
	%write tmp.cmd export Enable.2
	%write tmp.cmd export Disable.3
	%write tmp.cmd export MouseGetIntVect.4
	%write tmp.cmd export MouseRedetect.10
	%write tmp.cmd export WEP
	%write tmp.cmd name $(DBOXMPI_DRV)
	@wlink @tmp.cmd
	../../../tool/chgnever.pl 4.0 $(DBOXMPI_DRV)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

