# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_ISAPNP_LIB

C_SOURCE =    isapnp.c
OBJS =        $(SUBDIR)$(HPS)isapnp.obj $(SUBDIR)$(HPS)isapnp.obj $(SUBDIR)$(HPS)pnpiord.obj $(SUBDIR)$(HPS)pnpiowd.obj $(SUBDIR)$(HPS)pnpirqrd.obj $(SUBDIR)$(HPS)pnpirqwd.obj $(SUBDIR)$(HPS)pnpdmard.obj $(SUBDIR)$(HPS)pnpdmawd.obj $(SUBDIR)$(HPS)pnpdregr.obj $(SUBDIR)$(HPS)pnpdregw.obj $(SUBDIR)$(HPS)pnprdcfg.obj $(SUBDIR)$(HPS)pnpsrdpt.obj $(SUBDIR)$(HPS)pnpprdst.obj $(SUBDIR)$(HPS)pnptagst.obj $(SUBDIR)$(HPS)pnptpris.obj $(SUBDIR)$(HPS)pnpdevct.obj $(SUBDIR)$(HPS)pnpdevds.obj $(SUBDIR)$(HPS)pnptagso.obj $(SUBDIR)$(HPS)pnpinkey.obj $(SUBDIR)$(HPS)pnp32t16.obj $(SUBDIR)$(HPS)pnpirkey.obj $(SUBDIR)$(HPS)pnpwkcsn.obj $(SUBDIR)$(HPS)pnprdtag.obj $(SUBDIR)$(HPS)pnpdevtp.obj

!ifdef PC98
!else
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
DUMP_EXE =    $(SUBDIR)$(HPS)dump.$(EXEEXT)
!endif

$(HW_ISAPNP_LIB): $(OBJS)
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)isapnp.obj   -+$(SUBDIR)$(HPS)pnpiord.obj  -+$(SUBDIR)$(HPS)pnpiowd.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnpirqrd.obj -+$(SUBDIR)$(HPS)pnpirqwd.obj -+$(SUBDIR)$(HPS)pnpdmard.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnpdmawd.obj -+$(SUBDIR)$(HPS)pnpdregr.obj -+$(SUBDIR)$(HPS)pnpdregw.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnprdcfg.obj -+$(SUBDIR)$(HPS)pnpsrdpt.obj -+$(SUBDIR)$(HPS)pnpprdst.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnptagst.obj -+$(SUBDIR)$(HPS)pnptpris.obj -+$(SUBDIR)$(HPS)pnpdevct.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnpdevds.obj -+$(SUBDIR)$(HPS)pnptagso.obj -+$(SUBDIR)$(HPS)pnpinkey.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnp32t16.obj -+$(SUBDIR)$(HPS)pnpirkey.obj -+$(SUBDIR)$(HPS)pnpwkcsn.obj
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)pnprdtag.obj -+$(SUBDIR)$(HPS)pnpdevtp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_ISAPNP_LIB) .symbolic

exe: $(TEST_EXE) $(DUMP_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(SUBDIR)$(HPS)test.obj $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_CPU_LINK_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DUMP_EXE
$(DUMP_EXE): $(SUBDIR)$(HPS)dump.obj $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_FLATREAL_LIB) $(HW_FLATREAL_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(DUMP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)dump.obj $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LINK_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) name $(DUMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_ISAPNP_LIB)
          del tmp.cmd
          @echo Cleaning done

