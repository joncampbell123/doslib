# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../../../../../../.."
NOW_BUILDING = WOO_SORC_SET1_VRLTEST

TEST_EXE =     $(SUBDIR)$(HPS)test.$(EXEEXT)

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-103.27-103.47.mov.001.png.rsz.png.palunord.png.palord.png.pal palette.pal
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-103.27-103.47.mov.001.png.rsz.png.palunord.png.palord.png.vrl 0000.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-203.77-203.87.mov.001.png.rsz.png.palunord.png.palord.png.vrl 0001.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-203.77-203.87.mov.002.png.rsz.png.palunord.png.palord.png.vrl 0002.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-203.77-203.87.mov.003.png.rsz.png.palunord.png.palord.png.vrl 0003.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-73.07-73.27.mov.001.png.rsz.png.palunord.png.palord.png.vrl 0004.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-83.77-83.87.mov.003.png.rsz.png.palunord.png.palord.png.vrl 0005.vrl
	$(COPY) ..$(HPS)REDOGREEN2-colorkey-matte-alpha-360p.prores.mov-98.87-98.97.mov.001.png.rsz.png.palunord.png.palord.png.vrl 0006.vrl

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA_LIB) .symbolic
	
exe: $(TEST_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

