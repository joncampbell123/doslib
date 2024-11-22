# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -i"../../.."
NOW_BUILDING = HW_VGA_LIB

OBJS =        $(SUBDIR)$(HPS)libbmp.obj

LIBBMP_LIB =  ..$(HPS)..$(HPS)$(SUBDIR)$(HPS)libbmp.lib

$(LIBBMP_LIB):
	@cd ..$(HPS)..$(HPS)
	@$(MAKECMD) build lib $(SUBDIR)
	@cd $(HERE)

!ifndef PC98
! ifndef TARGET_WINDOWS
GENERAL_EXE =     $(SUBDIR)$(HPS)general.$(EXEEXT)
! endif
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(LIBBMP_LIB) .symbolic
	
exe: $(GENERAL_EXE) .symbolic

!ifdef GENERAL_EXE
$(GENERAL_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)general.obj
	%write tmp.cmd option quiet option map=$(GENERAL_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)general.obj name $(GENERAL_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_8.bmp
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
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

