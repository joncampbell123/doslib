# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = TESTS_CPU_INS
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

8086_EXE =    $(SUBDIR)$(HPS)8086.$(EXEEXT)

all: lib exe

lib: .symbolic

exe: $(8086_EXE) .symbolic

$(8086_EXE): $(SUBDIR)$(HPS)8086.obj
	%write tmp.cmd
	%append tmp.cmd option quiet option map=$(8086_EXE).map system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)8086.obj
	%append tmp.cmd name $(8086_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_SNDSB_LIB)
          del tmp.cmd
          @echo Cleaning done

