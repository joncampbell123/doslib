# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

# The build system we have has no effect on NASM (at the moment) and
# all memory models+CPU modes build the same binary. So why build them
# again and again? Build them only in dos86s mode.
!ifndef TARGET_WINDOWS
! ifndef TARGET_OS2
!  ifeq TARGET_MSDOS 16
!   ifeq TARGET86 86
!    ifeq MMODE s
BUILD_NASM_COM = 1
!    endif
!   endif
!  endif
! endif
!endif

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
AFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NASMFLAGS_THIS = 
NOW_BUILDING = DOSDRV_HELLO

!ifdef BUILD_NASM_COM
# MS-DOS COM output
HELLO_SYS =     $(SUBDIR)$(HPS)hello.sys
!endif

all: exe .symbolic

exe: $(HELLO_SYS) .symbolic

!ifdef BUILD_NASM_COM
$(HELLO_SYS): hello.asm
	nasm -o $@ -f bin $(NASMFLAGS) $[@
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_CPU_LIB)
          del tmp.cmd
          @echo Cleaning done

