
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -i$(SUBDIR)
NOW_BUILDING = HW_DOS_LIB

OBJS =        $(SUBDIR)$(HPS)dos.obj $(SUBDIR)$(HPS)dosxio.obj $(SUBDIR)$(HPS)dosxiow.obj $(SUBDIR)$(HPS)biosext.obj $(SUBDIR)$(HPS)himemsys.obj $(SUBDIR)$(HPS)emm.obj $(SUBDIR)$(HPS)dosbox.obj $(SUBDIR)$(HPS)biosmem.obj $(SUBDIR)$(HPS)biosmem3.obj $(SUBDIR)$(HPS)dosasm.obj $(SUBDIR)$(HPS)dosdlm16.obj $(SUBDIR)$(HPS)dosdlm32.obj $(SUBDIR)$(HPS)tgusmega.obj $(SUBDIR)$(HPS)tgussbos.obj $(SUBDIR)$(HPS)tgusumid.obj $(SUBDIR)$(HPS)dosntvdm.obj $(SUBDIR)$(HPS)doswin.obj $(SUBDIR)$(HPS)dos_lol.obj $(SUBDIR)$(HPS)dossmdrv.obj $(SUBDIR)$(HPS)dosvbox.obj $(SUBDIR)$(HPS)dosmapal.obj $(SUBDIR)$(HPS)dosflavr.obj $(SUBDIR)$(HPS)dos9xvm.obj $(SUBDIR)$(HPS)dos_nmi.obj $(SUBDIR)$(HPS)win32lrd.obj $(SUBDIR)$(HPS)win3216t.obj $(SUBDIR)$(HPS)win16vec.obj $(SUBDIR)$(HPS)dpmiexcp.obj $(SUBDIR)$(HPS)dosvcpi.obj $(SUBDIR)$(HPS)ddpmilin.obj $(SUBDIR)$(HPS)ddpmiphy.obj $(SUBDIR)$(HPS)ddpmidos.obj $(SUBDIR)$(HPS)ddpmidsc.obj $(SUBDIR)$(HPS)dpmirmcl.obj $(SUBDIR)$(HPS)dos_mcb.obj $(SUBDIR)$(HPS)dospsp.obj $(SUBDIR)$(HPS)dosdev.obj $(SUBDIR)$(HPS)dos_ltp.obj $(SUBDIR)$(HPS)dosdpmi.obj $(SUBDIR)$(HPS)dosdpfmc.obj $(SUBDIR)$(HPS)dosdpent.obj $(SUBDIR)$(HPS)dosvcpmp.obj $(SUBDIR)$(HPS)dosntmbx.obj $(SUBDIR)$(HPS)dosntwav.obj $(SUBDIR)$(HPS)doswinms.obj $(SUBDIR)$(HPS)dospwine.obj $(SUBDIR)$(HPS)dosdpmiv.obj $(SUBDIR)$(HPS)dosdpmev.obj $(SUBDIR)$(HPS)winemust.obj $(SUBDIR)$(HPS)fdosvstr.obj $(SUBDIR)$(HPS)w9xqthnk.obj $(SUBDIR)$(HPS)w16thelp.obj $(SUBDIR)$(HPS)dosntgtk.obj $(SUBDIR)$(HPS)dosntgvr.obj $(SUBDIR)$(HPS)dosntvld.obj $(SUBDIR)$(HPS)dosntvul.obj $(SUBDIR)$(HPS)dosntvin.obj $(SUBDIR)$(HPS)dosntvig.obj $(SUBDIR)$(HPS)dosntvi2.obj $(SUBDIR)$(HPS)dosw9xdv.obj $(SUBDIR)$(HPS)exeload.obj $(SUBDIR)$(HPS)execlsg.obj $(SUBDIR)$(HPS)exehdr.obj $(SUBDIR)$(HPS)exenertp.obj $(SUBDIR)$(HPS)exeneres.obj $(SUBDIR)$(HPS)exeneint.obj $(SUBDIR)$(HPS)exenesrl.obj $(SUBDIR)$(HPS)exenestb.obj $(SUBDIR)$(HPS)exenenet.obj $(SUBDIR)$(HPS)exenents.obj $(SUBDIR)$(HPS)exeneent.obj $(SUBDIR)$(HPS)exenew2x.obj $(SUBDIR)$(HPS)exenebmp.obj $(SUBDIR)$(HPS)exelest1.obj $(SUBDIR)$(HPS)exeletio.obj $(SUBDIR)$(HPS)exeleent.obj $(SUBDIR)$(HPS)exeleobt.obj $(SUBDIR)$(HPS)exeleopm.obj $(SUBDIR)$(HPS)exelefpt.obj $(SUBDIR)$(HPS)exelepar.obj $(SUBDIR)$(HPS)exelefrt.obj $(SUBDIR)$(HPS)exelevxd.obj $(SUBDIR)$(HPS)exelefxp.obj $(SUBDIR)$(HPS)exelehsz.obj $(SUBDIR)$(HPS)vectiret.obj $(SUBDIR)$(HPS)int2f.obj
!ifdef TARGET_WINDOWS
OBJS +=       $(SUBDIR)$(HPS)winfcon.obj $(SUBDIR)$(HPS)doswnseg.obj
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
!  ifeq TARGET_MSDOS 16
CLSGEXM1_DLM = $(SUBDIR)$(HPS)clsgexm1.dlm
CLSGEXT1_EXE = $(SUBDIR)$(HPS)clsgext1.$(EXEEXT)
!  endif
! endif
!endif

!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
BLAH_EXE =    $(SUBDIR)$(HPS)blah.$(EXEEXT)
ANSI_EXE =	  $(SUBDIR)$(HPS)ansi.$(EXEEXT)
LOL_EXE =     $(SUBDIR)$(HPS)lol.$(EXEEXT)
TESTSMRT_EXE =$(SUBDIR)$(HPS)testsmrt.$(EXEEXT)
HEXSTDI6_EXE =$(SUBDIR)$(HPS)hexstdi6.$(EXEEXT)
HEXSTDI2_EXE =$(SUBDIR)$(HPS)hexstdi2.$(EXEEXT)
HEXSTDIN_EXE =$(SUBDIR)$(HPS)hexstdin.$(EXEEXT)
NTASTRM_EXE = $(SUBDIR)$(HPS)ntastrm.$(EXEEXT)
INT16_EXE =   $(SUBDIR)$(HPS)int16.$(EXEEXT)
INT16P_EXE =  $(SUBDIR)$(HPS)int16p.$(EXEEXT)
!  ifeq TARGET_MSDOS 16
TESTDPMI_EXE =$(SUBDIR)$(HPS)testdpmi.$(EXEEXT)
!  endif
TSTHIMEM_EXE =$(SUBDIR)$(HPS)tsthimem.$(EXEEXT)
TESTBEXT_EXE =$(SUBDIR)$(HPS)testbext.$(EXEEXT)
TESTEMM_EXE = $(SUBDIR)$(HPS)testemm.$(EXEEXT)
TSTBIOM_EXE = $(SUBDIR)$(HPS)tstbiom.$(EXEEXT)
TSTLP_EXE =   $(SUBDIR)$(HPS)tstlp.$(EXEEXT)
! endif
!endif
TESTPRNA_EXE =$(SUBDIR)$(HPS)testprna.$(EXEEXT)
TESTPRNB_EXE =$(SUBDIR)$(HPS)testprnb.$(EXEEXT)
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
CR3_EXE =     $(SUBDIR)$(HPS)cr3.$(EXEEXT)

!ifdef TARGET_WINNT
EXEHDMP_EXE = $(SUBDIR)$(HPS)exehdmp.$(EXEEXT)
EXENEDMP_EXE = $(SUBDIR)$(HPS)exenedmp.$(EXEEXT)
EXENERDM_EXE = $(SUBDIR)$(HPS)exenerdm.$(EXEEXT)
EXENEEXP_EXE = $(SUBDIR)$(HPS)exeneexp.$(EXEEXT)
EXELEDMP_EXE = $(SUBDIR)$(HPS)exeledmp.$(EXEEXT)
EXEPEDMP_EXE = $(SUBDIR)$(HPS)exepedmp.$(EXEEXT)
!endif
!ifdef TARGET_WINDOWS
! ifeq TARGET_WINDOWS 40
EXEHDMP_EXE = $(SUBDIR)$(HPS)exehdmp.$(EXEEXT)
EXENEDMP_EXE = $(SUBDIR)$(HPS)exenedmp.$(EXEEXT)
EXENERDM_EXE = $(SUBDIR)$(HPS)exenerdm.$(EXEEXT)
EXENEEXP_EXE = $(SUBDIR)$(HPS)exeneexp.$(EXEEXT)
EXELEDMP_EXE = $(SUBDIR)$(HPS)exeledmp.$(EXEEXT)
EXEPEDMP_EXE = $(SUBDIR)$(HPS)exepedmp.$(EXEEXT)
! endif
!else
! ifndef TARGET_OS2
EXEHDMP_EXE = $(SUBDIR)$(HPS)exehdmp.$(EXEEXT)
EXENEDMP_EXE = $(SUBDIR)$(HPS)exenedmp.$(EXEEXT)
EXENERDM_EXE = $(SUBDIR)$(HPS)exenerdm.$(EXEEXT)
EXENEEXP_EXE = $(SUBDIR)$(HPS)exeneexp.$(EXEEXT)
EXELEDMP_EXE = $(SUBDIR)$(HPS)exeledmp.$(EXEEXT)
EXEPEDMP_EXE = $(SUBDIR)$(HPS)exepedmp.$(EXEEXT)
! endif
!endif

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
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos.obj      -+$(SUBDIR)$(HPS)biosext.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)himemsys.obj -+$(SUBDIR)$(HPS)emm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosbox.obj   -+$(SUBDIR)$(HPS)biosmem.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)biosmem3.obj -+$(SUBDIR)$(HPS)dosasm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)tgusmega.obj -+$(SUBDIR)$(HPS)tgussbos.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)tgusumid.obj -+$(SUBDIR)$(HPS)dosntvdm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)doswin.obj   -+$(SUBDIR)$(HPS)dosxio.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_lol.obj  -+$(SUBDIR)$(HPS)dossmdrv.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosvbox.obj  -+$(SUBDIR)$(HPS)dosmapal.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosflavr.obj -+$(SUBDIR)$(HPS)dos9xvm.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_nmi.obj  -+$(SUBDIR)$(HPS)win32lrd.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)win3216t.obj -+$(SUBDIR)$(HPS)win16vec.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dpmiexcp.obj -+$(SUBDIR)$(HPS)dosvcpi.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)ddpmilin.obj -+$(SUBDIR)$(HPS)ddpmiphy.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)ddpmidos.obj -+$(SUBDIR)$(HPS)ddpmidsc.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dpmirmcl.obj -+$(SUBDIR)$(HPS)dos_mcb.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dospsp.obj   -+$(SUBDIR)$(HPS)dosdev.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dos_ltp.obj  -+$(SUBDIR)$(HPS)dosdpmi.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosdpfmc.obj -+$(SUBDIR)$(HPS)dosdlm16.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosdlm32.obj -+$(SUBDIR)$(HPS)dosdpent.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosvcpmp.obj -+$(SUBDIR)$(HPS)dosntmbx.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosntwav.obj -+$(SUBDIR)$(HPS)doswinms.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dospwine.obj -+$(SUBDIR)$(HPS)dosdpmiv.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosdpmev.obj -+$(SUBDIR)$(HPS)winemust.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)fdosvstr.obj -+$(SUBDIR)$(HPS)w9xqthnk.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)w16thelp.obj -+$(SUBDIR)$(HPS)dosntgtk.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosntgvr.obj -+$(SUBDIR)$(HPS)dosntvld.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosntvul.obj -+$(SUBDIR)$(HPS)dosntvin.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosntvig.obj -+$(SUBDIR)$(HPS)dosntvi2.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)dosw9xdv.obj -+$(SUBDIR)$(HPS)exeload.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)execlsg.obj  -+$(SUBDIR)$(HPS)exehdr.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exenertp.obj -+$(SUBDIR)$(HPS)exeneres.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exeneint.obj -+$(SUBDIR)$(HPS)exenesrl.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exenestb.obj -+$(SUBDIR)$(HPS)exenenet.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exenents.obj -+$(SUBDIR)$(HPS)exeneent.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exenew2x.obj -+$(SUBDIR)$(HPS)exenebmp.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exelest1.obj -+$(SUBDIR)$(HPS)exeletio.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exeleent.obj -+$(SUBDIR)$(HPS)exeleobt.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exeleopm.obj -+$(SUBDIR)$(HPS)exelefpt.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exelepar.obj -+$(SUBDIR)$(HPS)exelefrt.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exelevxd.obj -+$(SUBDIR)$(HPS)exelefxp.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)exelehsz.obj -+$(SUBDIR)$(HPS)dosxiow.obj
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)vectiret.obj -+$(SUBDIR)$(HPS)int2f.obj
!ifdef TARGET_WINDOWS
	wlib -q -b -c $(HW_DOS_LIB) -+$(SUBDIR)$(HPS)winfcon.obj  -+$(SUBDIR)$(HPS)doswnseg.obj
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
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

exe: $(TESTSMRT_EXE) $(NTASTRM_EXE) $(TEST_EXE) $(CR3_EXE) $(TESTBEXT_EXE) $(TSTHIMEM_EXE) $(TESTEMM_EXE) $(TSTBIOM_EXE) $(LOL_EXE) $(TSTLP_EXE) $(TESTDPMI_EXE) $(INT16_EXE) $(INT16P_EXE) $(CLSGEXM1_DLM) $(CLSGEXT1_EXE) $(EXEHDMP_EXE) $(EXENEDMP_EXE) $(EXENEEXP_EXE) $(EXENERDM_EXE) $(EXELEDMP_EXE) $(EXEPEDMP_EXE) $(HEXSTDIN_EXE) $(HEXSTDI2_EXE) $(HEXSTDI6_EXE) $(ANSI_EXE) $(BLAH_EXE) $(TESTPRNB_EXE) $(TESTPRNA_EXE) .symbolic

lib: $(HW_DOS_LIB) .symbolic

!ifdef TESTSMRT_EXE
$(TESTSMRT_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testsmrt.obj
	%write tmp.cmd option quiet option map=$(TESTSMRT_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testsmrt.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTSMRT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef NTASTRM_EXE
$(NTASTRM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ntastrm.obj
	%write tmp.cmd option quiet option map=$(NTASTRM_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ntastrm.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(NTASTRM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DOSNTAST_VDD_BUILD
$(DOSNTAST_VDD): $(HW_DOS_LIB) $(HW_CPU_LIB) $(NTVDMLIB_LIB) $(NTVDMVDD_LIB) $(SUBDIR)$(HPS)dosntast.obj
	%write tmp.cmd option quiet system $(WLINK_DLL_SYSTEM) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(NTVDMVDD_LIB_WLINK_LIBRARIES) $(NTVDMLIB_LIB_WLINK_LIBRARIES) library winmm.lib file $(SUBDIR)$(HPS)dosntast.obj
	%write tmp.cmd option map=$(DOSNTAST_VDD).map
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
! ifdef DOSNTAST_VDD
# copy from Win32 dir. Build if necessary
winnt$(HPS)dosntast.vdd: dosntast.c
	@$(MAKECMD) build $@ winnt

$(DOSNTAST_VDD): winnt$(HPS)dosntast.vdd
	@$(COPY) winnt$(HPS)dosntast.vdd $(DOSNTAST_VDD)
! endif
!endif

!ifdef CLSGEXT1_EXE
$(CLSGEXT1_EXE): $(SUBDIR)$(HPS)clsgext1.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)clsgext1.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(CLSGEXT1_EXE).map
	%write tmp.cmd name $(CLSGEXT1_EXE)
	@wlink @tmp.cmd
!endif

!ifdef CLSGEXM1_DLM
$(CLSGEXM1_DLM): $(SUBDIR)$(HPS)clsgexm1.obj $(SUBDIR)$(HPS)clsgexm1.hdr.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file $(SUBDIR)$(HPS)clsgexm1.hdr.obj # header must COME FIRST
	%write tmp.cmd file $(SUBDIR)$(HPS)clsgexm1.obj
	%write tmp.cmd name $@
	%write tmp.cmd option map=$@.map
	@wlink @tmp.cmd

$(SUBDIR)$(HPS)clsgexm1.hdr.obj: $(SUBDIR)$(HPS)clsgexm1.hdr.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@

$(SUBDIR)$(HPS)clsgexm1.hdr.asm $(SUBDIR)$(HPS)clsgexm1.h: clsgexm1.def
	../../hw/dos/clsggen.pl --def $[@ --asm $@ --enum $(SUBDIR)$(HPS)clsgexm1.h

$(SUBDIR)$(HPS)clsgexm1.obj: clsgexm1.c $(SUBDIR)$(HPS)clsgexm1.hdr.obj $(SUBDIR)$(HPS)clsgexm1.h
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $(CFLAGS_CLSG) clsgexm1.c
	@$(CC) @tmp.cmd
!endif

!ifdef BLAH_EXE
$(BLAH_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)blah.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)blah.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(BLAH_EXE)
	%write tmp.cmd option map=$(BLAH_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef ANSI_EXE
$(ANSI_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ansi.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ansi.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(ANSI_EXE)
	%write tmp.cmd option map=$(ANSI_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef LOL_EXE
$(LOL_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)lol.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)lol.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(LOL_EXE)
	%write tmp.cmd option map=$(LOL_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTDPMI_EXE
$(TESTDPMI_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testdpmi.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testdpmi.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTDPMI_EXE)
	%write tmp.cmd option map=$(TESTDPMI_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXEHDMP_EXE
$(EXEHDMP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exehdmp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exehdmp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXEHDMP_EXE)
	%write tmp.cmd option map=$(EXEHDMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXEHDMP_EXE)
	@wbind $(EXEHDMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXEHDMP_EXE)
! endif
!endif

!ifdef EXENEDMP_EXE
$(EXENEDMP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exenedmp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exenedmp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXENEDMP_EXE)
	%write tmp.cmd option map=$(EXENEDMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXENEDMP_EXE)
	@wbind $(EXENEDMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXENEDMP_EXE)
! endif
!endif

!ifdef EXELEDMP_EXE
$(EXELEDMP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exeledmp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exeledmp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXELEDMP_EXE)
	%write tmp.cmd option map=$(EXELEDMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXELEDMP_EXE)
	@wbind $(EXELEDMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXELEDMP_EXE)
! endif
!endif

!ifdef EXENEEXP_EXE
$(EXENEEXP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exeneexp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exeneexp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXENEEXP_EXE)
	%write tmp.cmd option map=$(EXENEEXP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXENEEXP_EXE)
	@wbind $(EXENEEXP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXENEEXP_EXE)
! endif
!endif

!ifdef EXENERDM_EXE
$(EXENERDM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exenerdm.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exenerdm.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXENERDM_EXE)
	%write tmp.cmd option map=$(EXENERDM_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXENERDM_EXE)
	@wbind $(EXENERDM_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXENERDM_EXE)
! endif
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(DOSNTAST_VDD)
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)test.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(TEST_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
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
	%write tmp.cmd option map=$(CR3_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
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
	%write tmp.cmd option quiet option map=$(TSTLP_EXE).map system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tstlp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTLP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTBEXT_EXE
$(TESTBEXT_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testbext.obj
	%write tmp.cmd option quiet option map=$(TESTBEXT_EXE).map system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testbext.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTBEXT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TSTHIMEM_EXE
$(TSTHIMEM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tsthimem.obj
	%write tmp.cmd option quiet option map=$(TSTHIMEM_EXE).map system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tsthimem.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTHIMEM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTEMM_EXE
$(TESTEMM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testemm.obj
	%write tmp.cmd option quiet option map=$(TESTEMM_EXE).map system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testemm.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TESTEMM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TSTBIOM_EXE
$(TSTBIOM_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tstbiom.obj
	%write tmp.cmd option quiet option map=$(TSTBIOM_EXE).map system $(WLINK_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)tstbiom.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TSTBIOM_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef HEXSTDIN_EXE
$(HEXSTDIN_EXE): $(SUBDIR)$(HPS)hexstdin.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hexstdin.obj name $(HEXSTDIN_EXE)
	%write tmp.cmd option map=$(HEXSTDIN_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef HEXSTDI2_EXE
$(HEXSTDI2_EXE): $(SUBDIR)$(HPS)hexstdi2.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hexstdi2.obj name $(HEXSTDI2_EXE)
	%write tmp.cmd option map=$(HEXSTDI2_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef HEXSTDI6_EXE
$(HEXSTDI6_EXE): $(SUBDIR)$(HPS)hexstdi6.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)hexstdi6.obj name $(HEXSTDI6_EXE)
	%write tmp.cmd option map=$(HEXSTDI6_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef INT16_EXE
$(INT16_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)int16.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)int16.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(INT16_EXE)
	%write tmp.cmd option map=$(INT16_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef INT16P_EXE
$(INT16P_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)int16p.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)int16p.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(INT16P_EXE)
	%write tmp.cmd option map=$(INT16P_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXEPEDMP_EXE
$(EXEPEDMP_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)exepedmp.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)exepedmp.obj $(HW_DOS_LIB_WLINK_LIBRARIES) name $(EXEPEDMP_EXE)
	%write tmp.cmd option map=$(EXEPEDMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(EXEPEDMP_EXE)
	@wbind $(EXEPEDMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(EXEPEDMP_EXE)
! endif
!endif

!ifdef TESTPRNA_EXE
$(TESTPRNA_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testprna.obj $(DOSNTAST_VDD)
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testprna.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(TESTPRNA_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(TESTPRNA_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(TESTPRNA_EXE)
	@wbind $(TESTPRNA_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(TESTPRNA_EXE)
! endif
!endif

!ifdef TESTPRNB_EXE
$(TESTPRNB_EXE): $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)testprnb.obj $(DOSNTAST_VDD)
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)testprnb.obj $(HW_DOS_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(TESTPRNB_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD MOVEABLE DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE DISCARDABLE
	# protected mode only. real-mode Windows is a pain.
	%append tmp.cmd option protmode
!  endif
! endif
	%write tmp.cmd name $(TESTPRNB_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(TESTPRNB_EXE)
	@wbind $(TESTPRNB_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(TESTPRNB_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

