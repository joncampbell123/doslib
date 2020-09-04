# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../.."
NOW_BUILDING = DGJF2020

GAME_EXE =     $(SUBDIR)$(HPS)game.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA_LIB) .symbolic
	
exe: $(GAME_EXE) final .symbolic

final: $(GAME_EXE)
	rm -Rf final
	mkdir final
	cp -v dos86l/game.exe final/game.exe
	cp -v ../devasset/atomicplayboy-256x256.png final/atmpbrz.png
	cp -v ../devasset/woo-sorcerer-character/set2/palette.png.pal final/sorcwoo.pal
	cp -v ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-8.24-8.34.mov.001.png.cropped.png.palunord.png.palord.png.vrl final/sorcuhhh.vrl
	bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp -v ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcwoo$$i.vrl; done'
	bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp -v ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcbwo$$i.vrl; done'

!ifdef GAME_EXE
$(GAME_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)game.obj
	%write tmp.cmd option quiet option map=$(GAME_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)game.obj name $(GAME_EXE)
	@wlink @tmp.cmd
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

