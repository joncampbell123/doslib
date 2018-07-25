
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..$(HPS).. -zl -s
NOW_BUILDING = TOOL_LINKER_EX1

!ifneq TARGET_MSDOS 32
! ifdef TINYMODE
TEST_EXE =    $(SUBDIR)$(HPS)test.com
DOSLIBLINKER_OFMT = -of com
! else
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
TESTW_EXE =   $(SUBDIR)$(HPS)testw.exe
DOSLIBLINKER_OFMT = -of exe
! endif
!endif

DOSLIBLINKER = ../linux-host/lnkdos16

$(DOSLIBLINKER):
	make -C ..

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

.ASM.OBJ:
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe

exe: $(DOSLIBLINKER) $(TEST_EXE) $(TESTW_EXE) .symbolic

lib: .symbolic

!ifdef TINYMODE
WLINK_NOCLIBS_SYSTEM = dos com
!else
WLINK_NOCLIBS_SYSTEM = $(WLINK_SYSTEM)
!endif

!ifdef TESTW_EXE
$(TESTW_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj
	%write tmp.cmd option quiet OPTION NODEFAULTLIBS option map=$(TESTW_EXE).map system $(WLINK_NOCLIBS_SYSTEM) file $(SUBDIR)$(HPS)entry.obj file $(SUBDIR)$(HPS)drvc.obj name $(TESTW_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TEST_EXE) $(DOSLIBLINKER_OFMT)
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

