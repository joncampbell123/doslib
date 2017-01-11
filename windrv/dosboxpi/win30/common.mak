
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..$(HPS).. -s -zq -zl -zu -zw
NOW_BUILDING = WINDRV_DOSBOXPI_WIN31OW

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

$(SUBDIR)$(HPS)igrselio.obj: igrselio.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(NONDISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

$(SUBDIR)$(HPS)igregio.obj: igregio.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(NONDISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(DBOXMPI_DRV) .symbolic

lib: .symbolic

!ifdef DBOXMPI_DRV
$(DBOXMPI_DRV): $(SUBDIR)$(HPS)dboxmpi.obj $(SUBDIR)$(HPS)dllentry.obj $(SUBDIR)$(HPS)igprobe.obj $(SUBDIR)$(HPS)igreset.obj $(SUBDIR)$(HPS)igregio.obj $(SUBDIR)$(HPS)igrident.obj $(SUBDIR)$(HPS)igrselio.obj $(SUBDIR)$(HPS)iglib.obj $(SUBDIR)$(HPS)inthndlr.obj
	%write tmp.cmd option quiet
	%write tmp.cmd file $(SUBDIR)$(HPS)dllentry.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)igprobe.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)igreset.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)igregio.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)igrident.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)igrselio.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)iglib.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)dboxmpi.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)inthndlr.obj
	%write tmp.cmd option map=$(DBOXMPI_DRV).map
	%write tmp.cmd option osname='Windows 16-bit'
	%write tmp.cmd libpath %WATCOM%/lib286
	%write tmp.cmd libpath %WATCOM%/lib286/win
	%write tmp.cmd library windows
	%write tmp.cmd option nocaseexact
	%write tmp.cmd option stack=8k, heapsize=1k
	%write tmp.cmd format windows dll
	%write tmp.cmd segment TYPE CODE PRELOAD DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD DISCARDABLE SHARED
	%write tmp.cmd segment _NDTEXT PRELOAD FIXED SHARED
	%write tmp.cmd segment _NDDATA PRELOAD FIXED SHARED
	%write tmp.cmd option nodefaultlibs
	%write tmp.cmd option alignment=16
	%write tmp.cmd option version=3.0  # FIXME: Is Watcom's linker IGNORING THIS?
	%write tmp.cmd option modname=MOUSE
	%write tmp.cmd option description 'DOSBox-X Mouse Pointer Integration driver for Windows 3.x'
	%write tmp.cmd export Inquire.1
	%write tmp.cmd export Enable.2
	%write tmp.cmd export Disable.3
	%write tmp.cmd export MouseGetIntVect.4
	%write tmp.cmd name $(DBOXMPI_DRV)
	@wlink @tmp.cmd
	@wrc -31 $(DBOXMPI_DRV)
! ifdef WIN_NE_SETVER_BUILD
	../../../tool/chgnever.pl 3.0 $(DBOXMPI_DRV)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

