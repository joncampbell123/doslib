
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

# NTS: -g=DGROUP is needed because lnkdos16 treats objects after -seggroup as if they have their own independent segments,
#      and FIXUPs across segments with different bases are not allowed.
CFLAGS_END = -zl -s -zl -q -zu -zdp -zff -zgf -zc -fpi87 -dNEAR_DRVVAR -g=DGROUP
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
NOW_BUILDING = TOOL_LINKER_EX1

!ifdef TINYMODE
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
TEST_EXE =   $(SUBDIR)$(HPS)test.exe
TEST2_SYS =  $(SUBDIR)$(HPS)test2.sys
TEST2_EXE =  $(SUBDIR)$(HPS)test2.exe
!else
! ifeq TARGET_MSDOS 32
# DOSLIB linker cannot handle 32-bit OMF........yet
! else
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
TEST_EXE =   $(SUBDIR)$(HPS)test.exe
TEST2_SYS =  $(SUBDIR)$(HPS)test2.sys
TEST2_EXE =  $(SUBDIR)$(HPS)test2.exe
! endif
!endif

DOSLIBLINKER = ../linux-host/lnkdos16

$(DOSLIBLINKER):
	make -C ..

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $(CFLAGS_END) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

dos86t/drvci.obj: drvci.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $(CFLAGS_END) $[@
	@$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe

exe: $(DOSLIBLINKER) $(TEST_SYS) $(TEST_EXE) $(TEST2_SYS) $(TEST2_EXE) .symbolic

lib: .symbolic

!ifdef TINYMODE
WLINK_NOCLIBS_SYSTEM = dos com
!else
WLINK_NOCLIBS_SYSTEM = $(WLINK_SYSTEM)
!endif

drva.asm:
	../../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack --no-stub --ds-is-cs --int-stub
	echo "group DGROUP _END _BEGIN _TEXT _DATA" >>$@

!ifdef TEST_SYS
# TODO: dosdrv
$(TEST_SYS): $(SUBDIR)$(HPS)drva.obj $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)drva.obj -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -seggroup -sgname initsect -i $(SUBDIR)$(HPS)drvci.obj -o $(TEST_SYS) -of dosdrvrel -map $(TEST_SYS).map -segsym
!endif

!ifdef TEST_EXE
# TODO: dosdrv
$(TEST_EXE): $(SUBDIR)$(HPS)drva.obj $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj $(SUBDIR)$(HPS)exeep.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)drva.obj -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -seggroup -sgname initsect -i $(SUBDIR)$(HPS)drvci.obj -i $(SUBDIR)$(HPS)exeep.obj -o $(TEST_EXE) -of dosdrvexe -map $(TEST_EXE).map -segsym
!endif

# deliberately list the OBJ file with the DOS header later to see if the linker can correct for it

!ifdef TEST2_SYS
# TODO: dosdrv
$(TEST2_SYS): $(SUBDIR)$(HPS)drva.obj $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -i $(SUBDIR)$(HPS)drva.obj -seggroup -sgname initsect -i $(SUBDIR)$(HPS)drvci.obj -o $(TEST2_SYS) -of dosdrvrel -map $(TEST2_SYS).map -segsym
!endif

!ifdef TEST2_EXE
# TODO: dosdrv
$(TEST2_EXE): $(SUBDIR)$(HPS)drva.obj $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj $(SUBDIR)$(HPS)exeep.obj
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -i $(SUBDIR)$(HPS)drva.obj -seggroup -sgname initsect -i $(SUBDIR)$(HPS)drvci.obj -i $(SUBDIR)$(HPS)exeep.obj -o $(TEST2_EXE) -of dosdrvexe -map $(TEST2_EXE).map -segsym
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

