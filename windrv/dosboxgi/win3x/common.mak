
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.." -s -zq -zl -zu -zw -zc
NOW_BUILDING = WINDRV_DOSBOXIG_WIN31OW

DISCARDABLE_CFLAGS = -nt=_TEXT -nc=CODE
NONDISCARDABLE_CFLAGS = -nt=_NDTEXT -nc=NDCODE

DBOXIG_DRV = $(SUBDIR)$(HPS)dboxig.drv

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_DLL) $(DISCARDABLE_CFLAGS) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: lib exe

exe: $(DBOXIG_DRV) .symbolic

lib: .symbolic

!ifdef DBOXIG_DRV
$(DBOXIG_DRV): $(HW_DOSBOXID_LIB) $(SUBDIR)$(HPS)dboxig.obj $(SUBDIR)$(HPS)dllentry.obj
	%write tmp.cmd option quiet
	%write tmp.cmd file $(SUBDIR)$(HPS)dllentry.obj
	%write tmp.cmd file $(SUBDIR)$(HPS)dboxig.obj
	%write tmp.cmd $(HW_DOSBOXID_LIB_DRV_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(DBOXIG_DRV).map
	%write tmp.cmd option osname='Windows 16-bit'
	%write tmp.cmd libpath %WATCOM%/lib286
	%write tmp.cmd libpath %WATCOM%/lib286/win
	%write tmp.cmd library windows
	%write tmp.cmd option nocaseexact
	%write tmp.cmd option stack=8k, heapsize=1k
	%write tmp.cmd format windows dll
	%write tmp.cmd segment TYPE CODE PRELOAD DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED SHARED
	%write tmp.cmd option nodefaultlibs
	%write tmp.cmd option alignment=16
! ifeq TARGET_WINDOWS 30
	%write tmp.cmd option version=3.0  # FIXME: Is Watcom's linker IGNORING THIS?
! endif
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd option version=3.10  # FIXME: Is Watcom's linker IGNORING THIS?
! endif
	%write tmp.cmd option modname=MOUSE
! ifeq TARGET_WINDOWS 30
	%write tmp.cmd option description 'DOSBox-X Integrated Graphics driver for Windows 3.0'
! endif
! ifeq TARGET_WINDOWS 31
	%write tmp.cmd option description 'DOSBox-X Integrated Graphics driver for Windows 3.1'
! endif
	#%write tmp.cmd export Inquire.1
	%write tmp.cmd name $(DBOXIG_DRV)
	@wlink @tmp.cmd
! ifdef WIN_NE_SETVER_BUILD
!  ifeq TARGET_WINDOWS 20
	../../../tool/chgnever.pl 2.0 $(DBOXIG_DRV)
	../../../tool/win2xhdrpatch.pl $(DBOXIG_DRV)
	../../../tool/win2xalign512.pl $(DBOXIG_DRV)
	../../../tool/win2xstubpatch.pl $(DBOXIG_DRV)
!  endif
!  ifeq TARGET_WINDOWS 30
	../../../tool/chgnever.pl 3.0 $(DBOXIG_DRV)
!  endif
!  ifeq TARGET_WINDOWS 31
	../../../tool/chgnever.pl 3.10 $(DBOXIG_DRV)
!  endif
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

