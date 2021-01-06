
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_END = -zl -s -zl -q -zu -zdp -zff -zgf -zc -fpi87 -dNEAR_DRVVAR
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
NOW_BUILDING = TOOL_LINKER_EX1

!ifdef TINYMODE
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
!else
! ifeq TARGET_MSDOS 32
# DOSLIB linker cannot handle 32-bit OMF........yet
! else
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
! endif
!endif

DOSLIBLINKER = ../linux-host/lnkdos16

$(DOSLIBLINKER):
	make -C ..

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $(CFLAGS_END) $[@
	@$(CC) @tmp.cmd

.ASM.OBJ:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

dos86t/drvci.obj: drvci.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $(CFLAGS_END) $[@
	@$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe

exe: $(DOSLIBLINKER) $(TEST_SYS) .symbolic

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
	$(DOSLIBLINKER) -i $(SUBDIR)$(HPS)drva.obj -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -seggroup -i $(SUBDIR)$(HPS)drvci.obj -o $(TEST_SYS) -of dosdrv -map $(TEST_SYS).map
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

