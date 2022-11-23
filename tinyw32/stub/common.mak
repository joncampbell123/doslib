
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_DOS_LIB

!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
!  ifeq TARGET_MSDOS 16
!   ifeq TARGET86 86
!    ifeq MMODE s
BUILD_ASM=1
!    endif
!   endif
!  endif
! endif
!endif

!ifdef BUILD_ASM
STUB_EXE=$(SUBDIR)$(HPS)stub.exe
$(STUB_EXE): $(SUBDIR)$(HPS)stub.obj
	%write tmp.cmd option quiet system dos file $(SUBDIR)$(HPS)stub.obj
	%write tmp.cmd name $(SUBDIR)$(HPS)stub.exe
	@wlink @tmp.cmd

$(SUBDIR)$(HPS)stub.obj: stub.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

.asm.obj:
	nasm -o $@ -f obj $(NASMFLAGS) $[@

all: lib exe .symbolic

exe: $(HELLO_EXE) $(STUB_EXE) .symbolic

lib: .symbolic

clean: .symbolic
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

