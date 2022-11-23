# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_SMBIOS_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

C_SOURCE =    smbios.c
OBJS =        $(SUBDIR)$(HPS)smbios.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

$(HW_SMBIOS_LIB): $(OBJS)
	wlib -q -b -c $(HW_SMBIOS_LIB) -+$(SUBDIR)$(HPS)smbios.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_SMBIOS_LIB) .symbolic
	
exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(HW_FLATREAL_LIB) $(HW_FLATREAL_LIB_DEPENDENCIES) $(HW_SMBIOS_LIB) $(HW_SMBIOS_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp1.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_SMBIOS_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIS) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp1.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_SMBIOS_LIB)
          del tmp.cmd
          @echo Cleaning done

