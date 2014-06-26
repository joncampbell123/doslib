# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NOW_BUILDING = HW_ISAPNP_LIB

C_SOURCE =    isapnp.c
OBJS =        $(SUBDIR)$(HPS)isapnp.obj $(SUBDIR)$(HPS)isapnp.obj
TEST_EXE =    $(SUBDIR)$(HPS)test.exe

$(HW_ISAPNP_LIB): $(OBJS)
	wlib -q -b -c $(HW_ISAPNP_LIB) -+$(SUBDIR)$(HPS)isapnp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe

lib: $(HW_ISAPNP_LIB) .symbolic

exe: $(TEST_EXE) .symbolic

$(TEST_EXE): $(SUBDIR)$(HPS)test.obj $(HW_ISAPNP_LIB) $(HW_ISAPNP_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_CPU_LIB) $(HW_CPU_LIB_DEPENDENCIES)
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)test.obj $(HW_ISAPNP_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_CPU_LINK_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES) name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_ISAPNP_LIB)
          del tmp.cmd
          @echo Cleaning done

