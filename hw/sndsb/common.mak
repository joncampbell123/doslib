# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_SNDSB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    sndsb.c
OBJS =        $(SUBDIR)$(HPS)sndsb.obj $(SUBDIR)$(HPS)sbmixstr.obj $(SUBDIR)$(HPS)sbadpcm.obj $(SUBDIR)$(HPS)sbmixer.obj $(SUBDIR)$(HPS)sbmixerc.obj $(SUBDIR)$(HPS)sbmixnm.obj $(SUBDIR)$(HPS)sbenvbls.obj $(SUBDIR)$(HPS)sbdspio.obj $(SUBDIR)$(HPS)sbdsprst.obj $(SUBDIR)$(HPS)sbdspver.obj $(SUBDIR)$(HPS)sbtc1.obj $(SUBDIR)$(HPS)sbtc2.obj $(SUBDIR)$(HPS)sbdspcm1.obj $(SUBDIR)$(HPS)sbesscnm.obj $(SUBDIR)$(HPS)sbessreg.obj $(SUBDIR)$(HPS)sbdmabuf.obj $(SUBDIR)$(HPS)sbdspmst.obj $(SUBDIR)$(HPS)sbenum.obj $(SUBDIR)$(HPS)sbenumc.obj $(SUBDIR)$(HPS)sbnmi.obj $(SUBDIR)$(HPS)sbdacio.obj $(SUBDIR)$(HPS)sbgoldio.obj $(SUBDIR)$(HPS)sbsc400.obj $(SUBDIR)$(HPS)sbdspcpr.obj $(SUBDIR)$(HPS)sb16mres.obj $(SUBDIR)$(HPS)sbessprb.obj $(SUBDIR)$(HPS)sbmswinq.obj $(SUBDIR)$(HPS)sbirq.obj $(SUBDIR)$(HPS)sbnag.obj $(SUBDIR)$(HPS)sbcaps.obj $(SUBDIR)$(HPS)sbessply.obj $(SUBDIR)$(HPS)sbpirqf2.obj $(SUBDIR)$(HPS)sbpirq80.obj
OBJSPNP =     $(SUBDIR)$(HPS)sndsbpnp.obj
PNPCFG_EXE =  $(SUBDIR)$(HPS)pnpcfg.exe

!ifeq TARGET_MSDOS 16
! ifeq MMODE c
# this test program isn't going to fit in the compact memory model. sorry.
NO_TEST_ISAPNP=1
NO_TEST_EXE=1
! endif
! ifeq MMODE s
# yet again, doesn't fit into the small memory model (by 10 bytes)
NO_TEST_ISAPNP=1
#NO_TEST_EXE=1
! endif
!endif

!ifndef NO_TEST_EXE
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
!endif

$(HW_SNDSB_LIB): $(OBJS)
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sndsb.obj    -+$(SUBDIR)$(HPS)sbmixstr.obj -+$(SUBDIR)$(HPS)sbadpcm.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbmixer.obj  -+$(SUBDIR)$(HPS)sbmixnm.obj  -+$(SUBDIR)$(HPS)sbmixerc.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbenvbls.obj -+$(SUBDIR)$(HPS)sbdspio.obj  -+$(SUBDIR)$(HPS)sbdsprst.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbdspcm1.obj -+$(SUBDIR)$(HPS)sbesscnm.obj -+$(SUBDIR)$(HPS)sbessreg.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbdmabuf.obj -+$(SUBDIR)$(HPS)sbdspmst.obj -+$(SUBDIR)$(HPS)sbenum.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbgoldio.obj -+$(SUBDIR)$(HPS)sbsc400.obj  -+$(SUBDIR)$(HPS)sbdspcpr.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sb16mres.obj -+$(SUBDIR)$(HPS)sbessprb.obj -+$(SUBDIR)$(HPS)sbmswinq.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbenumc.obj  -+$(SUBDIR)$(HPS)sbnmi.obj    -+$(SUBDIR)$(HPS)sbdacio.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbdspver.obj -+$(SUBDIR)$(HPS)sbtc1.obj    -+$(SUBDIR)$(HPS)sbtc2.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbirq.obj    -+$(SUBDIR)$(HPS)sbnag.obj    -+$(SUBDIR)$(HPS)sbcaps.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbessply.obj -+$(SUBDIR)$(HPS)sbpirqf2.obj -+$(SUBDIR)$(HPS)sbpirq80.obj

$(HW_SNDSBPNP_LIB): $(OBJSPNP)
	wlib -q -b -c $(HW_SNDSBPNP_LIB) -+$(SUBDIR)$(HPS)sndsbpnp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe

lib: $(HW_SNDSB_LIB) $(HW_SNDSBPNP_LIB) .symbolic

exe: $(TEST_EXE) $(PNPCFG_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TEST_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

$(PNPCFG_EXE): $(WINDOWS_W9XVMM_LIB) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)pnpcfg.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)pnpcfg.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) name $(PNPCFG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_SNDSB_LIB)
          del tmp.cmd
          @echo Cleaning done

