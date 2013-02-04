# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8250_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    8250.c
OBJS =        $(SUBDIR)$(HPS)8250.obj
OBJSPNP =     $(SUBDIR)$(HPS)8250pnp.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
TESTPNP_EXE = $(SUBDIR)$(HPS)testpnp.exe

$(HW_8250_LIB): $(OBJS)
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250.obj

$(HW_8250PNP_LIB): $(OBJSPNP)
	wlib -q -b -c $(HW_8250PNP_LIB) -+$(SUBDIR)$(HPS)8250pnp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe
	
lib: $(HW_8250_LIB) $(HW_8250PNP_LIB) .symbolic
	
exe: $(TEST_EXE) $(TESTPNP_EXE) .symbolic

$(TEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENCIES) $(HW_8250_LIB) $(HW_8250_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp1.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_8250_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(TESTPNP_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENCIES) $(HW_8250_LIB) $(HW_8250_LIB_DEPENDENCIES) $(HW_8250PNP_LIB)  $(HW_8250PNP_LIB_DEPENDENCIES) $(HW_8250PBP_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(SUBDIR)$(HPS)testpnp.obj
	%write tmp2.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testpnp.obj $(HW_8250_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_8250PNP_LIB_WLINK_LIBRARIES) name $(TESTPNP_EXE)
	@wlink @tmp2.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8250_LIB)
          del tmp.cmd
          @echo Cleaning done

