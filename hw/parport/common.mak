# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_PARPORT_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    parport.c
OBJS =        $(SUBDIR)$(HPS)parport.obj
OBJSPNP =     $(SUBDIR)$(HPS)parpnp.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
TESTPNP_EXE = $(SUBDIR)$(HPS)testpnp.$(EXEEXT)
PRNT_EXE =    $(SUBDIR)$(HPS)prnt.$(EXEEXT)
PRNTPNP_EXE = $(SUBDIR)$(HPS)prntpnp.$(EXEEXT)
SAMPTEST_EXE =$(SUBDIR)$(HPS)samptest.$(EXEEXT)

$(HW_PARPORT_LIB): $(OBJS)
	wlib -q -b -c $(HW_PARPORT_LIB) -+$(SUBDIR)$(HPS)parport.obj

$(HW_PARPNP_LIB): $(OBJSPNP)
	wlib -q -b -c $(HW_PARPNP_LIB) -+$(SUBDIR)$(HPS)parpnp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_PARPORT_LIB) $(HW_PARPNP_LIB) .symbolic

exe: $(TEST_EXE) $(TESTPNP_EXE) $(PRNT_EXE) $(PRNTPNP_EXE) $(SAMPTEST_EXE) .symbolic

$(SAMPTEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)samptest.obj
	%write tmp1.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)samptest.obj $(HW_PARPORT_LIB_WLINK_LIBRARIES) $(HW_PARPNP_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) name $(SAMPTEST_EXE) option map=$(SAMPTEST_EXE).map
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp1.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_PARPORT_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TESTPNP_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(SUBDIR)$(HPS)testpnp.obj
	%write tmp2.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testpnp.obj $(HW_PARPORT_LIB_WLINK_LIBRARIES) $(HW_PARPNP_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) name $(TESTPNP_EXE) option map=$(TESTPNP_EXE).map
	@wlink @tmp2.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(PRNT_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)prnt.obj
	%write tmp1.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)prnt.obj $(HW_PARPORT_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(PRNT_EXE) option map=$(PRNT_EXE).map
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(PRNTPNP_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)prntpnp.obj
	%write tmp2.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)prntpnp.obj $(HW_PARPORT_LIB_WLINK_LIBRARIES) $(HW_PARPNP_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) name $(PRNTPNP_EXE) option map=$(PRNTPNP_EXE).map
	@wlink @tmp2.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_PARPORT_LIB)
          del tmp.cmd
          @echo Cleaning done

