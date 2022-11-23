# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_86DUINO_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    86duino.c
OBJS =        $(SUBDIR)$(HPS)86duino.obj $(SUBDIR)$(HPS)nb.obj $(SUBDIR)$(HPS)sb.obj $(SUBDIR)$(HPS)sb1.obj $(SUBDIR)$(HPS)mc.obj $(SUBDIR)$(HPS)gpio.obj $(SUBDIR)$(HPS)mcio.obj $(SUBDIR)$(HPS)cfg.obj $(SUBDIR)$(HPS)cfggpio.obj $(SUBDIR)$(HPS)mcm.obj $(SUBDIR)$(HPS)gpio_d.obj $(SUBDIR)$(HPS)gpio_pin.obj $(SUBDIR)$(HPS)gpio_a.obj $(SUBDIR)$(HPS)86uart.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
GPBLINK_EXE = $(SUBDIR)$(HPS)gpblink.$(EXEEXT)

$(HW_86DUINO_LIB): $(OBJS)
	wlib -q -b -c $(HW_86DUINO_LIB) -+$(SUBDIR)$(HPS)86duino.obj -+$(SUBDIR)$(HPS)nb.obj -+$(SUBDIR)$(HPS)sb.obj -+$(SUBDIR)$(HPS)sb1.obj -+$(SUBDIR)$(HPS)mc.obj -+$(SUBDIR)$(HPS)gpio.obj -+$(SUBDIR)$(HPS)mcio.obj -+$(SUBDIR)$(HPS)cfg.obj -+$(SUBDIR)$(HPS)cfggpio.obj -+$(SUBDIR)$(HPS)mcm.obj -+$(SUBDIR)$(HPS)gpio_d.obj -+$(SUBDIR)$(HPS)gpio_pin.obj -+$(SUBDIR)$(HPS)gpio_a.obj -+$(SUBDIR)$(HPS)86uart.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe
       
lib: $(HW_86DUINO_LIB) .symbolic

exe: $(TEST_EXE) $(GPBLINK_EXE) .symbolic

$(TEST_EXE): $(HW_86DUINO_LIB) $(HW_86DUINO_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) $(HW_86DUINO_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(GPBLINK_EXE): $(HW_86DUINO_LIB) $(HW_86DUINO_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)gpblink.obj $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet option map=$(GPBLINK_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)gpblink.obj $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) $(HW_86DUINO_LIB_WLINK_LIBRARIES) name $(GPBLINK_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

