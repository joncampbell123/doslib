
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.." -s -zq -zl -zu -zw -zc
NOW_BUILDING = WINDRV_PS2MOUSE_WIN31OW

DISCARDABLE_CFLAGS = -nt=_TEXT -nc=CODE
NONDISCARDABLE_CFLAGS = -nt=_NDTEXT -nc=NDCODE

PS2MOUSE_DRV = $(SUBDIR)$(HPS)ps2mouse.drv

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(DISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

$(SUBDIR)$(HPS)inthndlr.obj: inthndlr.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(NONDISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(PS2MOUSE_DRV) .symbolic

lib: .symbolic

!ifdef PS2MOUSE_DRV
$(PS2MOUSE_DRV): $(SUBDIR)$(HPS)ps2mouse.obj $(SUBDIR)$(HPS)dllentry.obj $(SUBDIR)$(HPS)inthndlr.obj
	%write tmp.cmd option quiet
	%write tmp.cmd file $(SUBDIR)$(HPS)dllentry.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)ps2mouse.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)inthndlr.obj
	%write tmp.cmd option map=$(PS2MOUSE_DRV).map
	%write tmp.cmd option osname='Windows 16-bit'
	%write tmp.cmd libpath %WATCOM%/lib286
	%write tmp.cmd libpath %WATCOM%/lib286/win
	%write tmp.cmd library windows
	%write tmp.cmd option nocaseexact
	%write tmp.cmd option stack=8k, heapsize=1k
	%write tmp.cmd format windows dll
	%write tmp.cmd segment TYPE CODE PRELOAD DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED SHARED
	%write tmp.cmd segment _NDTEXT PRELOAD FIXED SHARED
	%write tmp.cmd option nodefaultlibs
	%write tmp.cmd option alignment=16
! ifeq TARGET_WINDOWS 20
	%write tmp.cmd option version=2.0  # FIXME: Is Watcom's linker IGNORING THIS?
! endif
! ifeq TARGET_WINDOWS 30
	%write tmp.cmd option version=3.0  # FIXME: Is Watcom's linker IGNORING THIS?
! endif
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd option version=3.10  # FIXME: Is Watcom's linker IGNORING THIS?
! endif
	%write tmp.cmd option modname=MOUSE
! ifeq TARGET_WINDOWS 20
	%write tmp.cmd option description 'DOSLIB PS/2 Mouse driver for Windows 2.0'
! endif
! ifeq TARGET_WINDOWS 30
	%write tmp.cmd option description 'DOSLIB PS/2 Mouse driver for Windows 3.0'
! endif
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd option description 'DOSLIB PS/2 Mouse driver for Windows 3.1'
! endif
	%write tmp.cmd export Inquire.1
	%write tmp.cmd export Enable.2
	%write tmp.cmd export Disable.3
	%write tmp.cmd export MouseGetIntVect.4
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd export WEP
! endif
	%write tmp.cmd name $(PS2MOUSE_DRV)
	@wlink @tmp.cmd
	@wrc -31 $(PS2MOUSE_DRV)
! ifdef WIN_NE_SETVER_BUILD
!  ifeq TARGET_WINDOWS 20
	../../../tool/chgnever.pl 2.0 $(PS2MOUSE_DRV)
	../../../tool/win2xhdrpatch.pl $(PS2MOUSE_DRV)
	../../../tool/win2xalign512.pl $(PS2MOUSE_DRV)
	../../../tool/win2xstubpatch.pl $(PS2MOUSE_DRV)
!  endif
!  ifeq TARGET_WINDOWS 30
	../../../tool/chgnever.pl 3.0 $(PS2MOUSE_DRV)
!  endif
!  ifeq TARGET_WINDOWS 31
	../../../tool/chgnever.pl 3.10 $(PS2MOUSE_DRV)
!  endif
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

