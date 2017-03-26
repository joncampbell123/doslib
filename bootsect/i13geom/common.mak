
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

NOW_BUILDING = HW_BOOTSEC_I13GEOM

all: dos86t/boot1.img

dos86t/boot1.img: boot1.asm
	mkdir -p dos86t
	nasm -o $@ -f bin $[@

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_DOS_LIB)
          del tmp.cmd
          @echo Cleaning done

