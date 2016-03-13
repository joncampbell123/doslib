# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_DOSBOXID_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..

C_SOURCE =    iglib.c
OBJS =        $(SUBDIR)$(HPS)iglib.obj $(SUBDIR)$(HPS)iglib.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.exe

$(HW_DOSBOXID_LIB): $(OBJS)
	wlib -q -b -c $(HW_DOSBOXID_LIB) -+$(SUBDIR)$(HPS)iglib.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe
       
lib: $(HW_DOSBOXID_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_DOSBOXID_LIB) $(HW_DOSBOXID_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(HW_VGAGUI_LIB) $(HW_VGAGUI_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_PCI_LIB) $(HW_PCI_LIB_DEPENDENCIES) $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_DOSBOXID_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(HW_VGAGUI_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_PCI_LIB_WLINK_LIBRARIES) $(HW_ISAPNP_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOSBOXID_LIB)
          del tmp.cmd
          @echo Cleaning done

