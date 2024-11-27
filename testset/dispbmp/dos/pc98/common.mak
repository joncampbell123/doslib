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

!ifdef PC98
! ifndef TARGET_WINDOWS
640X2004_EXE =     $(SUBDIR)$(HPS)640x2004.$(EXEEXT)
640X4004_EXE =     $(SUBDIR)$(HPS)640x4004.$(EXEEXT)
640X400A_EXE =     $(SUBDIR)$(HPS)640x400a.$(EXEEXT)
640X480A_EXE =     $(SUBDIR)$(HPS)640x480a.$(EXEEXT)
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
	
exe: $(640X2004_EXE) $(640X4004_EXE) $(640X400A_EXE) $(640X480A_EXE) .symbolic

!ifdef 640X2004_EXE
$(640X2004_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x2004.obj
	%write tmp.cmd option quiet option map=$(640X2004_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x2004.obj name $(640X2004_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)200vga.bmp $(SUBDIR)$(HPS)640200_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X4004_EXE
$(640X4004_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x4004.obj
	%write tmp.cmd option quiet option map=$(640X4004_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x4004.obj name $(640X4004_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X400A_EXE
$(640X400A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x400a.obj
	%write tmp.cmd option quiet option map=$(640X400A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x400a.obj name $(640X400A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480A_EXE
$(640X480A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480a.obj
	%write tmp.cmd option quiet option map=$(640X480A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480a.obj name $(640X480A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

