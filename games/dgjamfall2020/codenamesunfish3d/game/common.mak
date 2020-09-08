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
	@rm -Rf final
	@mkdir final
	@cp dos86l/game.exe final/game.exe
	@cp ../devasset/winxp.png final/wxpbrz.png
	@cp ../devasset/atomicplayboy-256x256.png final/atmpbrz.png
	# Sorcerer palette. Uses only 32 colors.
	@dd if=../devasset/woo-sorcerer-character/set2/palette.png.pal of=final/sorcwoo.pal bs=3 count=32
	# VRLs
	@cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-8.24-8.34.mov.001.png.cropped.png.palunord.png.palord.png.vrl final/sorcuhhh.vrl
	@bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcwoo$$i.vrl; done'
	@bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcbwo$$i.vrl; done'
	# rotozoomer sin (quarter) table
	@cp sin2048.bin final/sorcwoo.sin
	# pack it up
	@bash -c 'cd final && ../dumbpack.pl sorcwoo.pal sorcwoo.sin sorcwoo{1,2,3,4,5,6,7,8,9}.vrl sorcuhhh.vrl sorcbwo{1,2,3,4,5,6,7,8,9}.vrl -- sorcwoo.vrp'
	@bash -c 'cd final && rm sorcwoo.pal sorcwoo.sin sorcwoo{1,2,3,4,5,6,7,8,9}.vrl sorcuhhh.vrl sorcbwo{1,2,3,4,5,6,7,8,9}.vrl'
	# fonts
	@cp ../devasset/arialfont/ariallarge_final.png final/ariallrg.png
	@cp ../devasset/arialfont/arialmed_final.png   final/arialmed.png
	@cp ../devasset/arialfont/arialsmall_final.png final/arialsml.png

!ifdef GAME_EXE
$(GAME_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)game.obj $(SUBDIR)$(HPS)unicode.obj $(SUBDIR)$(HPS)timer.obj $(SUBDIR)$(HPS)commtmp.obj $(SUBDIR)$(HPS)vrlimg.obj $(SUBDIR)$(HPS)vrlldf.obj $(SUBDIR)$(HPS)vrlldfd.obj $(SUBDIR)$(HPS)fzlibdec.obj $(SUBDIR)$(HPS)vmode.obj $(SUBDIR)$(HPS)vmodet.obj $(SUBDIR)$(HPS)vmode8bu.obj $(SUBDIR)$(HPS)fataexit.obj $(SUBDIR)$(HPS)fontbmp.obj $(SUBDIR)$(HPS)fontbmpd.obj $(SUBDIR)$(HPS)fontbmcu.obj
	%write tmp.cmd option quiet option map=$(GAME_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)game.obj file $(SUBDIR)$(HPS)unicode.obj file $(SUBDIR)$(HPS)timer.obj file $(SUBDIR)$(HPS)commtmp.obj file $(SUBDIR)$(HPS)vrlimg.obj file $(SUBDIR)$(HPS)vrlldf.obj file $(SUBDIR)$(HPS)vrlldfd.obj file $(SUBDIR)$(HPS)fzlibdec.obj file $(SUBDIR)$(HPS)vmode.obj file $(SUBDIR)$(HPS)vmodet.obj file $(SUBDIR)$(HPS)vmode8bu.obj file $(SUBDIR)$(HPS)fataexit.obj file $(SUBDIR)$(HPS)fontbmp.obj file $(SUBDIR)$(HPS)fontbmpd.obj file $(SUBDIR)$(HPS)fontbmcu.obj name $(GAME_EXE)
	@wlink @tmp.cmd
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

