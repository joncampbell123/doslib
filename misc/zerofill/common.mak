# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = MISC_ZEROFILL1_EXE
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."

ZEROFILL_EXE =  $(SUBDIR)$(HPS)zerofill.exe

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd

all: lib exe
	
lib: .symbolic

exe: $(ZEROFILL_EXE) .symbolic

$(ZEROFILL_EXE): $(HW_VGA_LIB) $(HW_DOS_LIB) $(HW_CPU_LIB) $(HW_LLMEM_LIB) $(HW_8042_LIB) $(HW_FLATREAL_LIB) $(SUBDIR)$(HPS)zerofill.obj ..$(HPS)..$(HPS)windows$(HPS)ntvdm$(HPS)$(SUBDIR)$(HPS)ntvdmlib.lib
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)zerofill.obj $(HW_DOS_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_CPU_LIB_WLINK_LIBRARIES) $(HW_FLATREAL_LIB_WLINK_LIBRARIES) $(HW_8042_LIB_WLINK_LIBRARIES) $(HW_LLMEM_LIB_WLINK_LIBRARIES) name $(ZEROFILL_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

