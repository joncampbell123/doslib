# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -i"../../.." -i"../../../.." -i"../../../../fmt/bmp"
NOW_BUILDING = TESTBMP_TEST

GENERAL1_EXE =     $(SUBDIR)$(HPS)general1.$(EXEEXT)
GENERAL1_RES =     $(SUBDIR)$(HPS)general1.res
GENERAL2_EXE =     $(SUBDIR)$(HPS)general2.$(EXEEXT)
GENERAL2_RES =     $(SUBDIR)$(HPS)general2.res

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(FMT_BMP_LIB) $(HW_DOS_LIB) .symbolic
	
exe: $(GENERAL1_EXE) $(GENERAL2_EXE) .symbolic

!ifdef GENERAL1_RES
$(GENERAL1_RES): general1.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)general1.res  $[@
!endif

!ifdef GENERAL2_RES
$(GENERAL2_RES): general2.rc
	$(RC) $(RCFLAGS_THIS) $(RCFLAGS) -fo=$(SUBDIR)$(HPS)general2.res  $[@
!endif

!ifdef GENERAL1_EXE
$(GENERAL1_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)general1.obj $(GENERAL1_RES)
	%write tmp.cmd option quiet option map=$(GENERAL1_EXE).map system $(WLINK_SYSTEM) library $(HW_DOS_LIB) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)general1.obj name $(GENERAL1_EXE)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
#      However if you want to use Open Watcom C runtime functions like sprintf() you have to declare them FIXED. Something
#      about how those C library functions are written don't work properly with MOVEABLE segments.
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED
!endif
!ifdef GENERAL1_RES
	%write tmp.cmd op resource=$(GENERAL1_RES) name $(GENERAL1_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GENERAL1_EXE)
	@wbind $(GENERAL1_EXE) -q -n
!endif
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)320200_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)240vga.bmp $(SUBDIR)$(HPS)320240_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)350vga.bmp $(SUBDIR)$(HPS)320350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vgatd.bmp $(SUBDIR)$(HPS)640480t8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247688.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w1280$(HPS)1024vga.bmp $(SUBDIR)$(HPS)128102_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w1600$(HPS)1200vga.bmp $(SUBDIR)$(HPS)160120_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)24bpp$(HPS)w640$(HPS)400.bmp $(SUBDIR)$(HPS)64040024.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)24bpp$(HPS)w640$(HPS)480.bmp $(SUBDIR)$(HPS)64048024.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)24bpp$(HPS)w800$(HPS)600.bmp $(SUBDIR)$(HPS)80060024.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)15bpp$(HPS)w640$(HPS)480.bmp $(SUBDIR)$(HPS)64048015.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)16bpp$(HPS)w640$(HPS)480.bmp $(SUBDIR)$(HPS)64048016.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)32bpp$(HPS)w640$(HPS)480.bmp $(SUBDIR)$(HPS)64048032.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)32bpp$(HPS)w640$(HPS)480a1.bmp $(SUBDIR)$(HPS)6404803A.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247684.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1280$(HPS)1024vga.bmp $(SUBDIR)$(HPS)128102_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)15bpp$(HPS)w640$(HPS)480bgr.bmp $(SUBDIR)$(HPS)640480r5.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)16bpp$(HPS)w640$(HPS)480bgr.bmp $(SUBDIR)$(HPS)640480r6.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480m.bmp $(SUBDIR)$(HPS)640480_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600m.bmp $(SUBDIR)$(HPS)800600_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef GENERAL2_EXE
$(GENERAL2_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)general2.obj $(GENERAL2_RES)
	%write tmp.cmd option quiet option map=$(GENERAL2_EXE).map system $(WLINK_SYSTEM) library $(HW_DOS_LIB) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)general2.obj name $(GENERAL2_EXE)
!ifeq TARGET_MSDOS 16
	%write tmp.cmd EXPORT WndProc.1 PRIVATE RESIDENT
# NTS: Real-mode Windows will NOT run our program unless segments are MOVEABLE DISCARDABLE. Especially Windows 2.x and 3.0.
#      However if you want to use Open Watcom C runtime functions like sprintf() you have to declare them FIXED. Something
#      about how those C library functions are written don't work properly with MOVEABLE segments.
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD FIXED
!endif
!ifdef GENERAL2_RES
	%write tmp.cmd op resource=$(GENERAL2_RES) name $(GENERAL2_EXE)
!endif
	@wlink @tmp.cmd
!ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(GENERAL2_EXE)
	@wbind $(GENERAL2_EXE) -q -n
!endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

