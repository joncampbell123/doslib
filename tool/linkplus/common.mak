
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = TOOL_LINKER

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

.CPP.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) -xs $[@
	@$(CXX) @tmp.cmd

all: lib exe .symbolic

exe: .symbolic

lib: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(FMT_OMF_LIB)
          del tmp.cmd
          @echo Cleaning done

