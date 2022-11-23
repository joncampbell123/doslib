# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_8250_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    8250.c
OBJS =        $(SUBDIR)$(HPS)8250.obj $(SUBDIR)$(HPS)8250prob.obj $(SUBDIR)$(HPS)8250bios.obj $(SUBDIR)$(HPS)8250siop.obj $(SUBDIR)$(HPS)8250fifo.obj $(SUBDIR)$(HPS)8250cint.obj $(SUBDIR)$(HPS)8250xien.obj $(SUBDIR)$(HPS)8250rcfg.obj $(SUBDIR)$(HPS)8250baud.obj $(SUBDIR)$(HPS)8250bauc.obj $(SUBDIR)$(HPS)8250tstr.obj $(SUBDIR)$(HPS)8250pstr.obj
OBJSPNP =     $(SUBDIR)$(HPS)8250pnp.obj $(SUBDIR)$(HPS)8250pnpa.obj

!ifdef PC98
!else
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
TESTPNP_EXE = $(SUBDIR)$(HPS)testpnp.$(EXEEXT)
!endif

$(HW_8250_LIB): $(OBJS)
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250.obj     -+$(SUBDIR)$(HPS)8250prob.obj
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250bios.obj -+$(SUBDIR)$(HPS)8250siop.obj
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250fifo.obj -+$(SUBDIR)$(HPS)8250cint.obj
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250xien.obj -+$(SUBDIR)$(HPS)8250rcfg.obj
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250baud.obj -+$(SUBDIR)$(HPS)8250bauc.obj
	wlib -q -b -c $(HW_8250_LIB) -+$(SUBDIR)$(HPS)8250tstr.obj -+$(SUBDIR)$(HPS)8250pstr.obj

$(HW_8250PNP_LIB): $(OBJSPNP)
	wlib -q -b -c $(HW_8250PNP_LIB) -+$(SUBDIR)$(HPS)8250pnp.obj -+$(SUBDIR)$(HPS)8250pnpa.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
	
lib: $(HW_8250_LIB) $(HW_8250PNP_LIB) .symbolic
	
exe: $(TEST_EXE) $(TESTPNP_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENCIES) $(HW_8250_LIB) $(HW_8250_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp1.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_8250_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTPNP_EXE
$(TESTPNP_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENCIES) $(HW_8250_LIB) $(HW_8250_LIB_DEPENDENCIES) $(HW_8250PNP_LIB)  $(HW_8250PNP_LIB_DEPENDENCIES) $(HW_8250PBP_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(SUBDIR)$(HPS)testpnp.obj
	%write tmp2.cmd option quiet option map=$(TESTPNP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)testpnp.obj $(HW_8250_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_8250PNP_LIB_WLINK_LIBRARIES) name $(TESTPNP_EXE)
	@wlink @tmp2.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_8250_LIB)
          del tmp.cmd
          @echo Cleaning done

