# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_SNDSB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

#DEBUG
#CFLAGS_THIS += -DDBG

C_SOURCE =    sndsb.c
OBJS =        $(SUBDIR)$(HPS)sndsb.obj $(SUBDIR)$(HPS)sbmixstr.obj $(SUBDIR)$(HPS)sbadpcm.obj $(SUBDIR)$(HPS)sbadpcms.obj $(SUBDIR)$(HPS)sbmixer.obj $(SUBDIR)$(HPS)sbmixerc.obj $(SUBDIR)$(HPS)sbmixnm.obj $(SUBDIR)$(HPS)sbenvbls.obj $(SUBDIR)$(HPS)sbdspio.obj $(SUBDIR)$(HPS)sbdspbio.obj $(SUBDIR)$(HPS)sbdsprst.obj $(SUBDIR)$(HPS)sbdspver.obj $(SUBDIR)$(HPS)sbtc1.obj $(SUBDIR)$(HPS)sbtc2.obj $(SUBDIR)$(HPS)sbdspcm1.obj $(SUBDIR)$(HPS)sbesscnm.obj $(SUBDIR)$(HPS)sbessreg.obj $(SUBDIR)$(HPS)sbdmabuf.obj $(SUBDIR)$(HPS)sbdmawch.obj $(SUBDIR)$(HPS)sbdspmst.obj $(SUBDIR)$(HPS)sbenum.obj $(SUBDIR)$(HPS)sbenumc.obj $(SUBDIR)$(HPS)sbnmi.obj $(SUBDIR)$(HPS)sbdacio.obj $(SUBDIR)$(HPS)sbgoldio.obj $(SUBDIR)$(HPS)sbsc400.obj $(SUBDIR)$(HPS)sbdspcpr.obj $(SUBDIR)$(HPS)sb16mres.obj $(SUBDIR)$(HPS)sbessprb.obj $(SUBDIR)$(HPS)sbmswinq.obj $(SUBDIR)$(HPS)sbirq.obj $(SUBDIR)$(HPS)sbnag.obj $(SUBDIR)$(HPS)sbcaps.obj $(SUBDIR)$(HPS)sbcaps2.obj $(SUBDIR)$(HPS)sbessply.obj $(SUBDIR)$(HPS)sbpirqc1.obj $(SUBDIR)$(HPS)sbpdmae2.obj $(SUBDIR)$(HPS)sbpdma14.obj $(SUBDIR)$(HPS)sbirqtst.obj $(SUBDIR)$(HPS)sbexaini.obj $(SUBDIR)$(HPS)sbcnaini.obj $(SUBDIR)$(HPS)sbhcdma.obj $(SUBDIR)$(HPS)sb16asp.obj $(SUBDIR)$(HPS)asp16rmp.obj $(SUBDIR)$(HPS)sbadpcm4.obj $(SUBDIR)$(HPS)sbadpc26.obj $(SUBDIR)$(HPS)sbadpcm2.obj $(SUBDIR)$(HPS)sbadpce4.obj $(SUBDIR)$(HPS)sbadpe26.obj $(SUBDIR)$(HPS)sbadpce2.obj $(SUBDIR)$(HPS)e2seq.obj $(SUBDIR)$(HPS)sb168051.obj
OBJSPNP =     $(SUBDIR)$(HPS)sndsbpnp.obj

!ifdef PC98
!else
PNPCFG_EXE =  $(SUBDIR)$(HPS)pnpcfg.$(EXEEXT)
!endif

!ifeq TARGET_MSDOS 16
! ifeq MMODE c
# this test program isn't going to fit in the compact memory model. sorry.
#NO_TEST_ISAPNP=1
#NO_TEST_EXE=1
! endif
! ifeq MMODE s
# yet again, doesn't fit into the small memory model (by 10 bytes)
#NO_TEST_ISAPNP=1
#NO_TEST_EXE=1
! endif
!endif

!ifndef NO_TEST_EXE
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
!endif

!ifeq TARGET_MSDOS 16
! ifdef TINYMODE
! else
!  ifeq MMODE c
!  else
TS_PI_EXE =	  $(SUBDIR)$(HPS)ts_pi.$(EXEEXT)
TS_PS_EXE =	  $(SUBDIR)$(HPS)ts_ps.$(EXEEXT)
TS_PS16_EXE = $(SUBDIR)$(HPS)ts_ps16.$(EXEEXT)
TS_SB16A_EXE =$(SUBDIR)$(HPS)ts_sb16a.$(EXEEXT)
TS_PD_EXE =   $(SUBDIR)$(HPS)ts_pd.$(EXEEXT)
E2SEQ_EXE =   $(SUBDIR)$(HPS)e2seq.$(EXEEXT)
!  endif
! endif
!endif
!ifeq TARGET_MSDOS 32
TS_PI_EXE =	  $(SUBDIR)$(HPS)ts_pi.$(EXEEXT)
TS_PS_EXE =	  $(SUBDIR)$(HPS)ts_ps.$(EXEEXT)
TS_PS16_EXE = $(SUBDIR)$(HPS)ts_ps16.$(EXEEXT)
TS_SB16A_EXE =$(SUBDIR)$(HPS)ts_sb16a.$(EXEEXT)
TS_PD_EXE =   $(SUBDIR)$(HPS)ts_pd.$(EXEEXT)
E2SEQ_EXE =   $(SUBDIR)$(HPS)e2seq.$(EXEEXT)
!endif

DBG16ASP_EXE = $(SUBDIR)$(HPS)dbg16asp.$(EXEEXT)

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
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbessply.obj -+$(SUBDIR)$(HPS)sbpirqc1.obj -+$(SUBDIR)$(HPS)sbpdma14.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbpdmae2.obj -+$(SUBDIR)$(HPS)sbirqtst.obj -+$(SUBDIR)$(HPS)sbexaini.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbhcdma.obj  -+$(SUBDIR)$(HPS)sb16asp.obj  -+$(SUBDIR)$(HPS)asp16rmp.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbdmawch.obj -+$(SUBDIR)$(HPS)sbdspbio.obj -+$(SUBDIR)$(HPS)sbcnaini.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbadpcms.obj -+$(SUBDIR)$(HPS)sbadpcm4.obj -+$(SUBDIR)$(HPS)sbadpc26.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbadpcm2.obj -+$(SUBDIR)$(HPS)sbadpce4.obj -+$(SUBDIR)$(HPS)sbadpe26.obj
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sbadpce2.obj -+$(SUBDIR)$(HPS)sbcaps2.obj  -+$(SUBDIR)$(HPS)sb168051.obj

$(HW_SNDSBPNP_LIB): $(OBJSPNP)
	wlib -q -b -c $(HW_SNDSBPNP_LIB) -+$(SUBDIR)$(HPS)sndsbpnp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_SNDSB_LIB) $(HW_SNDSBPNP_LIB) .symbolic

exe: $(TEST_EXE) $(TS_PS_EXE) $(TS_PI_EXE) $(TS_PS16_EXE) $(TS_SB16A_EXE) $(TS_PD_EXE) $(PNPCFG_EXE) $(DBG16ASP_EXE) $(E2SEQ_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TEST_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TS_PS_EXE
$(TS_PS_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ts_ps.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TS_PS_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ts_ps.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TS_PS_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TS_PS_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TS_PI_EXE
$(TS_PI_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ts_pi.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TS_PI_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ts_pi.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TS_PI_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TS_PI_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TS_PS16_EXE
$(TS_PS16_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ts_ps16.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TS_PS16_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ts_ps16.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TS_PS16_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TS_PS16_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TS_SB16A_EXE
$(TS_SB16A_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ts_sb16a.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TS_SB16A_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ts_sb16a.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TS_SB16A_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TS_SB16A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TS_PD_EXE
$(TS_PD_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)ts_pd.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(TS_PD_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ts_pd.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TS_PD_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(TS_PD_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DBG16ASP_EXE
$(DBG16ASP_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)dbg16asp.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(DBG16ASP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)dbg16asp.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_TEST_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(DBG16ASP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef PNPCFG_EXE
$(PNPCFG_EXE): $(WINDOWS_W9XVMM_LIB) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)pnpcfg.obj
	%write tmp.cmd option quiet option map=$(PNPCFG_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)pnpcfg.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) name $(PNPCFG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef E2SEQ_EXE
$(E2SEQ_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)e2seq.obj $(SUBDIR)$(HPS)ts_pcom.obj
	%write tmp.cmd
! ifeq TARGET_MSDOS 16 # Sound Blaster library needs more stack than default
	%append tmp.cmd option stack=4096
! endif
	%append tmp.cmd option quiet option map=$(E2SEQ_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)e2seq.obj file $(SUBDIR)$(HPS)ts_pcom.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES)
!  ifndef NO_E2SEQ_ISAPNP
	%append tmp.cmd $(HW_ISAPNP_LIB_WLINK_LIBRARIES)
!  endif
	%append tmp.cmd name $(E2SEQ_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_SNDSB_LIB)
          del tmp.cmd
          @echo Cleaning done

