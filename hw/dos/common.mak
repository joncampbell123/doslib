
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NOW_BUILDING = HW_DOS_LIB

OBJS =        $(SUBDIR)$(HPS)dos.obj $(SUBDIR)$(HPS)dosxio.obj $(SUBDIR)$(HPS)biosext.obj $(SUBDIR)$(HPS)himemsys.obj $(SUBDIR)$(HPS)emm.obj $(SUBDIR)$(HPS)dosbox.obj $(SUBDIR)$(HPS)biosmem.obj $(SUBDIR)$(HPS)biosmem3.obj $(SUBDIR)$(HPS)dosasm.obj $(SUBDIR)$(HPS)tgusmega.obj $(SUBDIR)$(HPS)tgussbos.obj $(SUBDIR)$(HPS)tgusumid.obj $(SUBDIR)$(HPS)dosntvdm.obj $(SUBDIR)$(HPS)doswin.obj $(SUBDIR)$(HPS)dos_lol.obj $(SUBDIR)$(HPS)dossmdrv.obj $(SUBDIR)$(HPS)dosvbox.obj $(SUBDIR)$(HPS)dosmapal.obj $(SUBDIR)$(HPS)dosflavr.obj $(SUBDIR)$(HPS)dos9xvm.obj $(SUBDIR)$(HPS)dos_nmi.obj $(SUBDIR)$(HPS)win32lrd.obj $(SUBDIR)$(HPS)win3216t.obj $(SUBDIR)$(HPS)win16vec.obj $(SUBDIR)$(HPS)dpmiexcp.obj $(SUBDIR)$(HPS)dosvcpi.obj $(SUBDIR)$(HPS)ddpmilin.obj $(SUBDIR)$(HPS)ddpmiphy.obj $(SUBDIR)$(HPS)ddpmidos.obj $(SUBDIR)$(HPS)ddpmidsc.obj $(SUBDIR)$(HPS)dpmirmcl.obj $(SUBDIR)$(HPS)dos_mcb.obj $(SUBDIR)$(HPS)dospsp.obj $(SUBDIR)$(HPS)dosdev.obj $(SUBDIR)$(HPS)dos_ltp.obj $(SUBDIR)$(HPS)dosdpmi.obj
!ifdef TARGET_WINDOWS
OBJS +=       $(SUBDIR)$(HPS)winfcon.obj
!endif

#HW_DOS_LIB =  $(SUBDIR)$(HPS)dos.lib

!ifndef TARGET_OS2
NTVDMLIB_LIB = ..$(HPS)..$(HPS)windows$(HPS)ntvdm$(HPS)$(SUBDIR)$(HPS)ntvdmlib.lib
NTVDMLIB_LIB_WLINK_LIBRARIES = library $(NTVDMLIB_LIB)
NTVDMVDD_LIB = ..$(HPS)..$(HPS)windows$(HPS)ntvdm$(HPS)$(SUBDIR)$(HPS)ntvdmvdd.lib
NTVDMVDD_LIB_WLINK_LIBRARIES = library $(NTVDMVDD_LIB)
!endif

!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
LOL_EXE =     $(SUBDIR)$(HPS)lol.exe
TESTSMRT_EXE =$(SUBDIR)$(HPS)testsmrt.exe
NTASTRM_EXE = $(SUBDIR)$(HPS)ntastrm.exe
!  ifeq TARGET_MSDOS 16
TESTDPMI_EXE =$(SUBDIR)$(HPS)testdpmi.exe
!  endif
TSTHIMEM_EXE =$(SUBDIR)$(HPS)tsthimem.exe
TESTBEXT_EXE =$(SUBDIR)$(HPS)testbext.exe
TESTEMM_EXE = $(SUBDIR)$(HPS)testemm.exe
TSTBIOM_EXE = $(SUBDIR)$(HPS)tstbiom.exe
TSTLP_EXE =   $(SUBDIR)$(HPS)tstlp.exe
! endif
!endif
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
CR3_EXE =     $(SUBDIR)$(HPS)cr3.exe

!ifndef TARGET_OS2
# if targeting Win32, then build the DOS NT assistant DLL that DOS versions
# can use to better interact with Windows NT/2000/XP. Else, copy the winnt
# DLL into the DOS build dir. The DLL is given the .VDD extension to clarify
# that it is intented for use as a VDD for NTVDM.EXE (or as a Win32 extension
# to the Win16 builds).
DOSNTAST_VDD = $(SUBDIR)$(HPS)dosntast.vdd
!endif

!ifdef TARGET_WINDOWS
! ifeq TARGET_MSDOS 32
!  ifeq TARGET_WINDOWS 40
DOSNTAST_VDD_BUILD=1
!  endif
! endif
!endif

$(HW_DOS_LIB): $(OBJS)
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos.obj -+$(SUBDIR)$(HPS)biosext.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)himemsys.obj -+$(SUBDIR)$(HPS)emm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosbox.obj -+$(SUBDIR)$(HPS)biosmem.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)biosmem3.obj -+$(SUBDIR)$(HPS)dosasm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)tgusmega.obj -+$(SUBDIR)$(HPS)tgussbos.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)tgusumid.obj -+$(SUBDIR)$(HPS)dosntvdm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)doswin.obj -+$(SUBDIR)$(HPS)dosxio.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_lol.obj -+$(SUBDIR)$(HPS)dossmdrv.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosvbox.obj -+$(SUBDIR)$(HPS)dosmapal.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosflavr.obj -+$(SUBDIR)$(HPS)dos9xvm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_nmi.obj -+$(SUBDIR)$(HPS)win32lrd.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)win3216t.obj -+$(SUBDIR)$(HPS)win16vec.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dpmiexcp.obj -+$(SUBDIR)$(HPS)dosvcpi.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)ddpmilin.obj -+$(SUBDIR)$(HPS)ddpmiphy.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)ddpmidos.obj -+$(SUBDIR)$(HPS)ddpmidsc.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dpmirmcl.obj -+$(SUBDIR)$(HPS)dos_mcb.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dospsp.obj -+$(SUBDIR)$(HPS)dosdev.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_ltp.obj -+$(SUBDIR)$(HPS)dosdpmi.obj
!ifdef TARGET_WINDOWS
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)winfcon.obj
!endif

# some components need a 386 in real mode
$(SUBDIR)$(HPS)biosmem3.obj: biosmem3.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS386) $[@
	@$(CC) @tmp.cmd

$(SUBDIR)$(HPS)dosntast.obj: dosntast.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

.ASM.OBJ:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: lib exe

exe: $(TESTSMRT_EXE) $(NTASTRM_EXE) $(TEST_EXE) $(CR3_EXE) $(TESTBEXT_EXE) $(TSTHIMEM_EXE) $(TESTEMM_EXE) $(TSTBIOM_EXE) $(LOL_EXE) $(TSTLP_EXE) $(TESTDPMI_EXE) .symbolic

lib: $(DOSNTAST_VDD) $(HW_DOS_LIB) .symbolic

!ifdef TESTSMRT_EXE
$(TESTSMRT_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testsmrt.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testsmrt.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTSMRT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef NTASTRM_EXE
$(NTASTRM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ntastrm.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ntastrm.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(NTASTRM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DOSNTAST_VDD_BUILD
$(DOSNTAST_VDD): $(HW_DOS_LIB) $(HW_CPU_LIB) $(NTVDMLIB_LIB) $(NTVDMVDD_LIB) $(SUBDIR)$(HPS)dosntast.obj
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(NTVDMVDD_LIB_WLINK_LIBRARIES) $(NTVDMLIB_LIB_WLINK_LIBRARIES) library winmm.lib file $(SUBDIR)$(HPS)dosntast.obj
	%write tmp.cmd option modname='DOSNTAST'
! ifeq TARGET_MSDOS 32
	%write tmp.cmd option nostdcall
! endif
# explanation: if we use the IMPLIB option, Watcom will go off and make an import library that
# cases all references to refer to HELLDLD1.DLL within the NE image, which Windows does NOT like.
# we need to ensure the DLL name is encoded by itself without a .DLL extension which is more
# compatible with Windows and it's internal functions.
#
# Frankly I'm surprised that Watcom has this bug considering how long it's been around... Kind of disappointed really
#	%write tmp.cmd option impfile=$(SUBDIR)$(HPS)DOSNTAST.LCF
	%write tmp.cmd name $(DOSNTAST_VDD)
	@wlink @tmp.cmd
!else
# copy from Win32 dir. Build if necessary
winnt$(HPS)dosntast.vdd: dosntast.c
	@$(MAKECMD) build lib winnt

! ifdef DOSNTAST_VDD
$(DOSNTAST_VDD): winnt$(HPS)dosntast.vdd
	@$(COPY) winnt$(HPS)dosntast.vdd $(DOSNTAST_VDD)
! endif
!endif

!ifdef LOL_EXE
$(LOL_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)lol.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)lol.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(LOL_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTDPMI_EXE
$(TESTDPMI_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testdpmi.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testdpmi.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTDPMI_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)test.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(SUBDIR)$(HPS)test.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(TEST_EXE)
	@wbind $(TEST_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(TEST_EXE)
! endif
!endif

!ifdef CR3_EXE
$(CR3_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)cr3.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)cr3.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(SUBDIR)$(HPS)cr3.map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(CR3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(CR3_EXE)
	@wbind $(CR3_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(CR3_EXE)
! endif
!endif

!ifdef TSTLP_EXE
$(TSTLP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tstlp.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tstlp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTLP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTBEXT_EXE
$(TESTBEXT_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testbext.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testbext.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTBEXT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TSTHIMEM_EXE
$(TSTHIMEM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tsthimem.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tsthimem.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTHIMEM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTEMM_EXE
$(TESTEMM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testemm.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testemm.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTEMM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TSTBIOM_EXE
$(TSTBIOM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tstbiom.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tstbiom.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTBIOM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

