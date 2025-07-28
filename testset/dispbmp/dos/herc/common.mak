# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -i"../../.." -i"../../../../fmt/bmp"
NOW_BUILDING = TESTBMP_TEST

!ifndef PC98
! ifndef TARGET_WINDOWS
640X350A_EXE =     $(SUBDIR)$(HPS)640x350a.$(EXEEXT)
720X350A_EXE =     $(SUBDIR)$(HPS)720x350a.$(EXEEXT)
720X350I_EXE =     $(SUBDIR)$(HPS)720x350i.$(EXEEXT)
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
       
lib: $(FMT_BMP_LIB) .symbolic

exe: $(640X350A_EXE) $(720X350A_EXE) $(720X350I_EXE) .symbolic

!ifdef 640X350A_EXE
$(640X350A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x350a.obj
	%write tmp.cmd option quiet option map=$(640X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x350a.obj name $(640X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)350m.bmp $(SUBDIR)$(HPS)640350.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 720X350A_EXE
$(720X350A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)720x350a.obj
	%write tmp.cmd option quiet option map=$(720X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)720x350a.obj name $(720X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w720$(HPS)350m.bmp $(SUBDIR)$(HPS)720350.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 720X350I_EXE
$(720X350I_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)720x350i.obj
	%write tmp.cmd option quiet option map=$(720X350I_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)720x350i.obj name $(720X350I_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w720$(HPS)350ega.bmp $(SUBDIR)$(HPS)720350c.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

