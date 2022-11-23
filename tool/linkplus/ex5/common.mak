
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.." -zl -s
NOW_BUILDING = TOOL_LINKER_EX2

!ifdef TINYMODE
TEST_EXE =   $(SUBDIR)$(HPS)test.com
TEST2_EXE =  $(SUBDIR)$(HPS)test2.com
TEST3_EXE =  $(SUBDIR)$(HPS)test3.com
TEST4_EXE =  $(SUBDIR)$(HPS)test4.com
DOSLIBLINKER_OFMT = -of comrel
!else
! ifeq TARGET_MSDOS 32
# DOSLIB linker cannot handle 32-bit OMF........yet
! else
TEST_EXE =   $(SUBDIR)$(HPS)test.exe
TEST2_EXE =  $(SUBDIR)$(HPS)test2.exe
TEST3_EXE =  $(SUBDIR)$(HPS)test3.exe
TEST4_EXE =  $(SUBDIR)$(HPS)test4.exe
DOSLIBLINKER_OFMT = -of exe
! endif
!endif

DOSLIBLINKER = ../linux-host/lnkdos16

$(DOSLIBLINKER):
	make -C ..

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: lib exe

exe: $(DOSLIBLINKER) $(TEST_EXE) $(TEST2_EXE) $(TEST3_EXE) $(TEST4_EXE) .symbolic

lib: .symbolic

!ifdef TINYMODE
WLINK_NOCLIBS_SYSTEM = dos com
!else
WLINK_NOCLIBS_SYSTEM = $(WLINK_SYSTEM)
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(SUBDIR)$(HPS)entry.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -o $(TEST_EXE) $(DOSLIBLINKER_OFMT) -map $(TEST_EXE).map -segsym
!endif

!ifdef TEST2_EXE
$(TEST2_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)entry2.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)entry2.obj -o $(TEST2_EXE) $(DOSLIBLINKER_OFMT) -map $(TEST2_EXE).map -segsym
!endif

!ifdef TEST3_EXE
$(TEST3_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)entry2.obj $(SUBDIR)$(HPS)entry3.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry2.obj -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)entry3.obj -o $(TEST3_EXE) $(DOSLIBLINKER_OFMT) -map $(TEST3_EXE).map -segsym
!endif

!ifdef TEST4_EXE
$(TEST4_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)entry2.obj $(SUBDIR)$(HPS)entry3.obj $(SUBDIR)$(HPS)entry4.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry2.obj -i $(SUBDIR)$(HPS)entry3.obj -i $(SUBDIR)$(HPS)entry4.obj -i $(SUBDIR)$(HPS)entry.obj -o $(TEST4_EXE) $(DOSLIBLINKER_OFMT) -map $(TEST4_EXE).map -segsym
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

