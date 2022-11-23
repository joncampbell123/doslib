
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

!ifdef TINYMODE
CFLAGS_END = -zl -s -zl -q -zu -zdp -zff -zgf -zc -fpi87 -dNEAR_DRVVAR
DOSLINKFMT = -of dosdrv
DOSLINKFMT2 = -of dosdrvrel
!else
DOSLINKFMT = -of dosdrvexe
DOSLINKFMT2 = -of dosdrvrel
!endif

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../.."
NOW_BUILDING = TOOL_LINKER_EX1

!ifdef TINYMODE
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
TEST2_SYS =  $(SUBDIR)$(HPS)test2.sys

DRVA =		 $(SUBDIR)$(HPS)drva.obj
!else
! ifeq TARGET_MSDOS 32
# DOSLIB linker cannot handle 32-bit OMF........yet
! else
TEST_SYS =   $(SUBDIR)$(HPS)test.sys
TEST2_SYS =  $(SUBDIR)$(HPS)test2.sys

DRVA =		 $(SUBDIR)$(HPS)drva2.obj
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
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $(CFLAGS_END) -nt=_INITTEXT -nc=INITCODE $[@
	@$(CC) @tmp.cmd

all: $(OMFSEGDG) lib exe

exe: $(DOSLIBLINKER) $(TEST_SYS) $(TEST2_SYS) .symbolic

lib: .symbolic

!ifdef TINYMODE
WLINK_NOCLIBS_SYSTEM = dos com
!else
WLINK_NOCLIBS_SYSTEM = $(WLINK_SYSTEM)
!endif

drva.asm:
	../../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack --no-stub --ds-is-cs --int-stub
	echo "group DGROUP _END _BEGIN _TEXT _DATA" >>$@

drva2.asm:
	../../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack --no-stub --int-stub-far
	echo "group DGROUP _END _BEGIN _DATA" >>$@

!ifdef TEST_SYS
$(TEST_SYS): $(DRVA) $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj
	$(DOSLIBLINKER) -i $(DRVA) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -i $(SUBDIR)$(HPS)drvci.obj -o $(TEST_SYS) $(DOSLINKFMT) -map $(TEST_SYS).map
!endif

!ifdef TEST2_SYS
$(TEST2_SYS): $(DRVA) $(SUBDIR)$(HPS)entry.obj $(SUBDIR)$(HPS)drvc.obj $(SUBDIR)$(HPS)drvci.obj
	$(DOSLIBLINKER) -i $(DRVA) -i $(SUBDIR)$(HPS)entry.obj -i $(SUBDIR)$(HPS)drvc.obj -i $(SUBDIR)$(HPS)drvci.obj -o $(TEST2_SYS) $(DOSLINKFMT2) -map $(TEST2_SYS).map
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

