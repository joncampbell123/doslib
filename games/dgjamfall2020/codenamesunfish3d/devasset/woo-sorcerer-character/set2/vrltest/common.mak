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
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.001.png.cropped.png.palunord.png.palord.png.pal				palette.pal
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.001.png.cropped.png.palunord.png.palord.png.vrl 			0000.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.002.png.cropped.png.palunord.png.palord.png.vrl				0001.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.003.png.cropped.png.palunord.png.palord.png.vrl				0002.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.004.png.cropped.png.palunord.png.palord.png.vrl				0003.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.005.png.cropped.png.palunord.png.palord.png.vrl				0004.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.006.png.cropped.png.palunord.png.palord.png.vrl				0005.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.007.png.cropped.png.palunord.png.palord.png.vrl				0006.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.008.png.cropped.png.palunord.png.palord.png.vrl				0007.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-101.30-119.43.mov-small-wooo.mov-1.43-1.82.mov.009.png.cropped.png.palunord.png.palord.png.vrl				0008.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.001.png.cropped.png.palunord.png.palord.png.vrl		0009.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.002.png.cropped.png.palunord.png.palord.png.vrl		0010.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.003.png.cropped.png.palunord.png.palord.png.vrl		0011.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.004.png.cropped.png.palunord.png.palord.png.vrl		0012.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.005.png.cropped.png.palunord.png.palord.png.vrl		0013.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.006.png.cropped.png.palunord.png.palord.png.vrl		0014.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.007.png.cropped.png.palunord.png.palord.png.vrl		0015.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.008.png.cropped.png.palunord.png.palord.png.vrl		0016.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-23.64-23.96.mov.009.png.cropped.png.palunord.png.palord.png.vrl		0017.vrl
	$(COPY) ..$(HPS)REDOWIDER1-colorkey-matte-alpha-720p.mov-199.52-225.61.mov-uhhhhhh-wooooooo.mov-8.24-8.34.mov.001.png.cropped.png.palunord.png.palord.png.vrl		0018.vrl

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

