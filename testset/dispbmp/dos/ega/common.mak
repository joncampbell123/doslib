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
640X200L_EXE =     $(SUBDIR)$(HPS)640x200l.$(EXEEXT)
640X200M_EXE =     $(SUBDIR)$(HPS)640x200m.$(EXEEXT)
640X350L_EXE =     $(SUBDIR)$(HPS)640x350l.$(EXEEXT)
640X350M_EXE =     $(SUBDIR)$(HPS)640x350m.$(EXEEXT)
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
	
exe: $(640X200L_EXE) $(640X200M_EXE) $(640X350L_EXE) $(640X350M_EXE) .symbolic

!ifdef 640X200L_EXE
$(640X200L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x200l.obj
	%write tmp.cmd option quiet option map=$(640X200L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x200l.obj name $(640X200L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)200cga.bmp $(SUBDIR)$(HPS)64020016.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X200M_EXE
$(640X200M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x200m.obj
	%write tmp.cmd option quiet option map=$(640X200M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x200m.obj name $(640X200M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)200vga.bmp $(SUBDIR)$(HPS)640200mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)200m.bmp $(SUBDIR)$(HPS)640200m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X350L_EXE
$(640X350L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x350l.obj
	%write tmp.cmd option quiet option map=$(640X350L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x350l.obj name $(640X350L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)350ega.bmp $(SUBDIR)$(HPS)64035016.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X350M_EXE
$(640X350M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x350m.obj
	%write tmp.cmd option quiet option map=$(640X350M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x350m.obj name $(640X350M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)350m.bmp $(SUBDIR)$(HPS)640350m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

