# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../.."
NOW_BUILDING = DGJF2020

PARTO1_EXE =     $(SUBDIR)$(HPS)parto1.$(EXEEXT)
GAME_COM =		 $(SUBDIR)$(HPS)game.com

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

exe: $(PARTO1_EXE) $(GAME_COM) final .symbolic

final: $(PARTO1_EXE) $(GAME_COM)
	@rm -Rf final
	@mkdir final
	@cp dos86l/game.com final/game.com
	@cp dos86l/parto1.exe final/parto1.exe
	@cp ../devasset/winxp.png final/wxpbrz.png
	@cp ../devasset/towncenter.png final/twnctr.png
	@cp ../devasset/atomicplayboy-256x256.png final/atmpbrz.png
	@cp ../devasset/godtemplestonebehance.320x200.png final/tmplie.png
	# Sorcerer palette. Uses only 32 colors.
	@dd if=../devasset/woo-sorcerer-character/set2/palette.png.pal of=final/sorcwoo.pal bs=3 count=32
	# VRLs
	@cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-8.24-8.34.mov.001.png.cropped.png.palunord.png.palord.png.vrl final/sorcuhhh.vrl
	@bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcwoo$$i.vrl; done'
	@bash -c 'for i in 1 2 3 4 5 6 7 8 9; do cp ../devasset/woo-sorcerer-character/set2/REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.00$$i.png.cropped.png.palunord.png.palord.png.vrl final/sorcbwo$$i.vrl; done'
	@bash -c 'for i in 1 2 3 4; do cp ../devasset/gmch$$i.vrl final/gmch$$i.vrl; done'
	@bash -c 'for i in 1 2 3 4; do dd if=../devasset/gmch$$i.pal of=final/gmch$$i.pal count=16 bs=3; done'
	@bash -c 'for i in 1 2; do cp ../devasset/gmchm$$i.vrl final/gmchm$$i.vrl; done'
	@bash -c 'for i in 3; do cp ../devasset/"gmch$$i"oco.vrl final/"gmch$$i"oco.vrl; done'
	# rotozoomer sin (quarter) table
	@cp sin2048.bin final/sorcwoo.sin
	# pack it up
	@bash -c 'cd final && ../dumbpack.pl sorcwoo.pal sorcwoo.sin sorcwoo{1,2,3,4,5,6,7,8,9}.vrl sorcuhhh.vrl sorcbwo{1,2,3,4,5,6,7,8,9}.vrl gmch{1,2,3,4}.pal gmch{1,2,3,4}.vrl gmchm{1,2}.vrl gmch3oco.vrl -- sorcwoo.vrp'
	@bash -c 'cd final && rm sorcwoo.pal sorcwoo.sin sorcwoo{1,2,3,4,5,6,7,8,9}.vrl sorcuhhh.vrl sorcbwo{1,2,3,4,5,6,7,8,9}.vrl'
	@bash -c 'cd final && rm gmch{1,2,3,4}.{vrl,pal} gmchm{1,2}.vrl gmch3oco.vrl'
	# fonts
	@cp ../devasset/arialfont/ariallarge_final.png final/ariallrg.png
	@cp ../devasset/arialfont/arialmed_final.png   final/arialmed.png
	@cp ../devasset/arialfont/arialsmall_final.png final/arialsml.png
	# wall textures
	@bash -c 'cd final && for i in 1 2 3 4; do cp ../../devasset/walltx0/watx000$$i.24bpp.png.palunord.png.palord.png watx000$$i.png; done'

!ifdef GAME_COM
$(GAME_COM): game.asm
	nasm -o $(GAME_COM) -f bin -l $(GAME_COM).lst game.asm
!endif

!ifdef PARTO1_EXE
$(PARTO1_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_8259_LIB) $(HW_8259_LIB_DEPENDENCIES) $(FMT_MINIPNG_LIB) $(FMT_MINIPNG_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)parto1.obj $(SUBDIR)$(HPS)unicode.obj $(SUBDIR)$(HPS)timer.obj $(SUBDIR)$(HPS)commtmp.obj $(SUBDIR)$(HPS)vrlimg.obj $(SUBDIR)$(HPS)vrlldf.obj $(SUBDIR)$(HPS)vrlldfd.obj $(SUBDIR)$(HPS)fzlibdec.obj $(SUBDIR)$(HPS)vmode.obj $(SUBDIR)$(HPS)vmodet.obj $(SUBDIR)$(HPS)vmode8bu.obj $(SUBDIR)$(HPS)fataexit.obj $(SUBDIR)$(HPS)fontbmp.obj $(SUBDIR)$(HPS)fontbmpd.obj $(SUBDIR)$(HPS)fontbmcu.obj $(SUBDIR)$(HPS)fbvga8ud.obj $(SUBDIR)$(HPS)f_aria_l.obj $(SUBDIR)$(HPS)f_aria_m.obj $(SUBDIR)$(HPS)f_aria_s.obj $(SUBDIR)$(HPS)vut8u.obj $(SUBDIR)$(HPS)sin2048.obj $(SUBDIR)$(HPS)dumbpack.obj $(SUBDIR)$(HPS)sorcpack.obj $(SUBDIR)$(HPS)rotzfx8u.obj $(SUBDIR)$(HPS)rotzimg.obj $(SUBDIR)$(HPS)rotzpng8.obj $(SUBDIR)$(HPS)dbgheap.obj $(SUBDIR)$(HPS)vrldraw.obj $(SUBDIR)$(HPS)seqcanvs.obj $(SUBDIR)$(HPS)seqcomm.obj $(SUBDIR)$(HPS)freein1.obj $(SUBDIR)$(HPS)cs_intro.obj $(SUBDIR)$(HPS)keyboard.obj
	%write tmp.cmd option quiet option map=$(PARTO1_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8259_LIB_WLINK_LIBRARIES) $(FMT_MINIPNG_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)parto1.obj file $(SUBDIR)$(HPS)unicode.obj file $(SUBDIR)$(HPS)timer.obj file $(SUBDIR)$(HPS)commtmp.obj file $(SUBDIR)$(HPS)vrlimg.obj file $(SUBDIR)$(HPS)vrlldf.obj file $(SUBDIR)$(HPS)vrlldfd.obj file $(SUBDIR)$(HPS)fzlibdec.obj file $(SUBDIR)$(HPS)vmode.obj file $(SUBDIR)$(HPS)vmodet.obj file $(SUBDIR)$(HPS)vmode8bu.obj file $(SUBDIR)$(HPS)fataexit.obj file $(SUBDIR)$(HPS)fontbmp.obj file $(SUBDIR)$(HPS)fontbmpd.obj file $(SUBDIR)$(HPS)fontbmcu.obj file $(SUBDIR)$(HPS)fbvga8ud.obj file $(SUBDIR)$(HPS)f_aria_l.obj file $(SUBDIR)$(HPS)f_aria_m.obj file $(SUBDIR)$(HPS)f_aria_s.obj file $(SUBDIR)$(HPS)vut8u.obj file $(SUBDIR)$(HPS)sin2048.obj file $(SUBDIR)$(HPS)dumbpack.obj file $(SUBDIR)$(HPS)sorcpack.obj file $(SUBDIR)$(HPS)rotzfx8u.obj file $(SUBDIR)$(HPS)rotzimg.obj file $(SUBDIR)$(HPS)rotzpng8.obj file $(SUBDIR)$(HPS)dbgheap.obj file $(SUBDIR)$(HPS)vrldraw.obj file $(SUBDIR)$(HPS)seqcanvs.obj file $(SUBDIR)$(HPS)seqcomm.obj file $(SUBDIR)$(HPS)freein1.obj file $(SUBDIR)$(HPS)cs_intro.obj file $(SUBDIR)$(HPS)keyboard.obj name $(PARTO1_EXE)
	@wlink @tmp.cmd
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

