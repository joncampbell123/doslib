
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.." -s -zq -zl -zu -zw -zc
CFLAGS_THIS_GCC = -I.. -I../../.. -nostdlib -O3 -Os -fomit-frame-pointer -fno-exceptions -fno-pic -std=gnu99 -ffreestanding -fno-hosted -Wall -pedantic

NOW_BUILDING = WINDRV_DOSBOXPI_WIN9XOW

CFLAGS_VXD = -e=2 -zq -zw -mf -oilrtfm -wx -fp3 -3r -dTARGET_MSDOS=32 -dTARGET_WINDOWS=32 -dTARGET86=386 -DMMODE=f -q -bc -zl -zdp

CFLAGS_VXD_GCC = -march=i386 -DTARGET_MSDOS=32 -DTARGET_WINDOWS=32 -DTARGET86=386 -DMMODE=f -Wall -pedantic

DISCARDABLE_CFLAGS = -nt=_TEXT -nc=CODE
NONDISCARDABLE_CFLAGS = -nt=_NDTEXT -nc=NDCODE

DBOXMPI_VXD = $(SUBDIR)$(HPS)dboxmpi.vxd
DBOXMPI_DRV = $(SUBDIR)$(HPS)dboxmpi.drv

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

# TODO: temporary disabled, need to fixed
all: .symbolic #lib exe

exe: $(DBOXMPI_DRV) $(DBOXMPI_VXD) .symbolic

lib: .symbolic

../../../hw/dosboxid/win32_vxd/dosboxid.lib:
	cd ../../../hw/dosboxid && ./make.sh build lib win32

$(SUBDIR)$(HPS)dboxvxd.o: dboxvxd.c
	%write tmp.cmd $(CFLAGS_THIS_GCC) $(CFLAGS_VXD_GCC) $[@
	@$(GCC32) -E -o $(SUBDIR)/dboxvxd.pp -c @tmp.cmd
	%write tmp.cmd $(CFLAGS_THIS_GCC) $(CFLAGS_VXD_GCC) $[@
	@$(GCC32) -o $(SUBDIR)/dboxvxd.int.o -c @tmp.cmd
	@strip --strip-debug --strip-unneeded -R .comment -R .note.GNU-stack -R .eh_frame $(SUBDIR)/dboxvxd.int.o
	# ready to puke yet?
	@$(GCCLD32) -m elf_i386 -r -o win313l/dboxvxd.o win313l/dboxvxd.int.o -T vxdld.ldscript

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

!ifdef DBOXMPI_VXD
$(DBOXMPI_VXD): $(HW_DOSBOXID_LIB) ../../../hw/dosboxid/win32_vxd/dosboxid.lib $(SUBDIR)$(HPS)dboxvxd.o
	%write tmp.cmd option quiet
	%write tmp.cmd system win_vxd
	%write tmp.cmd file $(SUBDIR)$(HPS)dboxvxd.o
	%write tmp.cmd library ../../../hw/dosboxid/win32_vxd/dosboxid.lib
	%write tmp.cmd option map=$(DBOXMPI_VXD).map
	%write tmp.cmd option nocaseexact
	%write tmp.cmd segment CLASS 'CODE' PRELOAD NONDISCARDABLE
	%write tmp.cmd segment CLASS 'ICODE' DISCARDABLE
	%write tmp.cmd segment CLASS 'PCODE' NONDISCARDABLE
	%write tmp.cmd option nodefaultlibs
	%write tmp.cmd option modname=DBOXMPI
	%write tmp.cmd option description 'DOSBox-X Mouse Pointer Integration driver for Windows 95'
	%write tmp.cmd export DBOXMPI_DDB.1=DBOXMPI_DDB
	%write tmp.cmd name $(DBOXMPI_VXD)
	@wlink @tmp.cmd
	# using GCC object files seems to throw off Watcom's ability to link it's code to VXD properly
	../../../tool/fixvxdsdkver.pl  $(DBOXMPI_VXD) 0x0000 0x030A
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

