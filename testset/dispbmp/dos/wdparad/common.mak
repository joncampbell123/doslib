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
640X480A_EXE =     $(SUBDIR)$(HPS)640x480a.$(EXEEXT)
640X480L_EXE =     $(SUBDIR)$(HPS)640x480l.$(EXEEXT)
640X480M_EXE =     $(SUBDIR)$(HPS)640x480m.$(EXEEXT)
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
	
exe: $(640X480A_EXE) $(640X480L_EXE) $(640X480M_EXE) .symbolic

!ifdef 640X480A_EXE
$(640X480A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480a.obj
	%write tmp.cmd option quiet option map=$(640X480A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480a.obj name $(640X480A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480L_EXE
$(640X480L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480l.obj
	%write tmp.cmd option quiet option map=$(640X480L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480l.obj name $(640X480L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480M_EXE
$(640X480M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480m.obj
	%write tmp.cmd option quiet option map=$(640X480M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480m.obj name $(640X480M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480m.bmp $(SUBDIR)$(HPS)640480_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

