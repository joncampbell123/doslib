# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_MB_INTEL_PIIX3_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../.."

C_SOURCE =    piix3.c
OBJS =        $(SUBDIR)$(HPS)piix3.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)

$(HW_MB_INTEL_PIIX3_LIB): $(OBJS)
	wlib -q -b -c $(HW_MB_INTEL_PIIX3_LIB) -+$(SUBDIR)$(HPS)piix3.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

lib: $(HW_MB_INTEL_PIIX3_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(HW_MB_INTEL_PIIX3_LIB) $(HW_MB_INTEL_PIIX3_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_MB_INTEL_PIIX3_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) name $(TEST_EXE) option map=$(TEST_EXE).map
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_MB_INTEL_PIIX3_LIB)
          del tmp.cmd
          @echo Cleaning done

