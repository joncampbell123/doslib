# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_ACPI_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    acpi.c
OBJS =        $(SUBDIR)$(HPS)acpi.obj $(SUBDIR)$(HPS)acpi.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
ACPIDUMP_EXE =$(SUBDIR)$(HPS)acpidump.$(EXEEXT)

$(HW_ACPI_LIB): $(OBJS)
	wlib -q -b -c $(HW_ACPI_LIB) -+$(SUBDIR)$(HPS)acpi.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_ACPI_LIB) .symbolic

exe: $(TEST_EXE) $(ACPIDUMP_EXE) .symbolic

$(TEST_EXE): $(HW_ACPI_LIB) $(HW_ACPI_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_FLATREAL_LIB) $(HW_FLATREAL_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_ACPI_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

$(ACPIDUMP_EXE): $(HW_ACPI_LIB) $(HW_ACPI_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_FLATREAL_LIB) $(HW_FLATREAL_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)acpidump.obj
	%write tmp.cmd option quiet option map=$(ACPIDUMP_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)acpidump.obj $(HW_ACPI_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) name $(ACPIDUMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

