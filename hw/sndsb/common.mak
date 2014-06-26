# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_SNDSB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    sndsb.c
OBJS =        $(SUBDIR)$(HPS)sndsb.obj
OBJSPNP =     $(SUBDIR)$(HPS)sndsbpnp.obj
PNPCFG_EXE =  $(SUBDIR)$(HPS)pnpcfg.exe

!ifeq TARGET_MSDOS 16
! ifeq MMODE c
# I'm tired of stripping the features down just to make the compact builds work
NO_TEST_EXE=1
! endif
! ifeq MMODE s
# I'm tired of stripping the features down just to make the small builds work
NO_TEST_EXE=1
! endif
!endif

!ifndef NO_TEST_EXE
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
!endif

$(HW_SNDSB_LIB): $(OBJS)
	wlib -q -b -c $(HW_SNDSB_LIB) -+$(SUBDIR)$(HPS)sndsb.obj

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
$(TEST_EXE): $(WINDOWS_W9XVMM_LIB) $(WINDOWS_W9XVMM_LIB_DEPENDENCIES) $(HW_SNDSB_LIB) $(HW_SNDSB_LIB_DEPENDENCIES) $(HW_SNDSBPNP_LIB) $(HW_SNDSBPNP_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_SNDSB_LIB_WLINK_LIBRARIES) $(HW_SNDSBPNP_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(WINDOWS_W9XVMM_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
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

