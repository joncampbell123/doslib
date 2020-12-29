
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.." -zl -s
NOW_BUILDING = TOOL_LINKER_EX1

!ifdef TINYMODE
TEST_EXE =    $(SUBDIR)$(HPS)test.com
TEST2_EXE =   $(SUBDIR)$(HPS)test2.com
TEST3_EXE =   $(SUBDIR)$(HPS)test3.com
TEST4_EXE =   $(SUBDIR)$(HPS)test4.com
TESTA_EXE =   $(SUBDIR)$(HPS)testa.com
TESTA2_EXE =  $(SUBDIR)$(HPS)testa2.com
TESTB_EXE =   $(SUBDIR)$(HPS)testb.com
TESTB2_EXE =  $(SUBDIR)$(HPS)testb2.com
DOSLIBLINKER_OFMT = -of com
!else
TEST_EXE =    $(SUBDIR)$(HPS)test.exe
TEST2_EXE =   $(SUBDIR)$(HPS)test2.exe
! ifeq TARGET_MSDOS 32
# DOSLIB linker cannot handle 32-bit OMF........yet
! else
TEST3_EXE =   $(SUBDIR)$(HPS)test3.exe
TEST4_EXE =   $(SUBDIR)$(HPS)test4.com
TESTA_EXE =   $(SUBDIR)$(HPS)testa.exe
TESTA2_EXE =  $(SUBDIR)$(HPS)testa2.exe
TESTB_EXE =   $(SUBDIR)$(HPS)testb.exe
TESTB2_EXE =  $(SUBDIR)$(HPS)testb2.exe
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

exe: $(DOSLIBLINKER) $(TEST_EXE) $(TEST2_EXE) $(TEST3_EXE) $(TEST4_EXE) $(TESTA_EXE) $(TESTA2_EXE) $(TESTB_EXE) $(TESTB2_EXE) .symbolic

lib: .symbolic

!ifdef TINYMODE
WLINK_NOCLIBS_SYSTEM = dos com
!else
WLINK_NOCLIBS_SYSTEM = $(WLINK_SYSTEM)
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj
	%write tmp.cmd option quiet OPTION NODEFAULTLIBS option map=$(TEST_EXE).map system $(WLINK_NOCLIBS_SYSTEM) file $(SUBDIR)$(HPS)entry.obj file $(SUBDIR)$(HPS)drvc.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTA_EXE
$(TESTA_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TESTA_EXE) -com0 $(DOSLIBLINKER_OFMT) -map $(TESTA_EXE).map
!endif

!ifdef TESTB_EXE
$(TESTB_EXE): $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TESTB_EXE) -com0 $(DOSLIBLINKER_OFMT) -pflat -map $(TESTB_EXE).map
!endif

!ifdef TEST2_EXE
$(TEST2_EXE): $(SUBDIR)$(HPS)entry2.obj $(SUBDIR)$(HPS)drvc.obj
	%write tmp.cmd option quiet OPTION NODEFAULTLIBS option map=$(TEST2_EXE).map system $(WLINK_NOCLIBS_SYSTEM) file $(SUBDIR)$(HPS)entry2.obj file $(SUBDIR)$(HPS)drvc.obj name $(TEST2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TESTA2_EXE
$(TESTA2_EXE): $(SUBDIR)$(HPS)entry2.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry2.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TESTA2_EXE) -com100 $(DOSLIBLINKER_OFMT) -map $(TESTA2_EXE).map
!endif

!ifdef TESTB2_EXE
$(TESTB2_EXE): $(SUBDIR)$(HPS)entry2.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry2.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TESTB2_EXE) -com100 $(DOSLIBLINKER_OFMT) -pflat -map $(TESTB2_EXE).map
!endif

!ifdef TEST3_EXE
$(TEST3_EXE): $(SUBDIR)$(HPS)entry3.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry3.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TEST3_EXE) -com100 $(DOSLIBLINKER_OFMT) -pflat -map $(TEST3_EXE).map
!endif

!ifdef TEST4_EXE
$(TEST4_EXE): $(SUBDIR)$(HPS)entry3.obj $(SUBDIR)$(HPS)drvc.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry2.obj -i $(SUBDIR)$(HPS)drvc.obj -o $(TEST4_EXE) -com100 -of comrel -pflat -map $(TEST4_EXE).map
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

