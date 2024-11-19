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
320X200A_EXE =     $(SUBDIR)$(HPS)320x200a.$(EXEEXT)
320X200B_EXE =     $(SUBDIR)$(HPS)320x200b.$(EXEEXT)
320X240A_EXE =     $(SUBDIR)$(HPS)320x240a.$(EXEEXT)
320X350A_EXE =     $(SUBDIR)$(HPS)320x350a.$(EXEEXT)
320X400A_EXE =     $(SUBDIR)$(HPS)320x400a.$(EXEEXT)
320X480A_EXE =     $(SUBDIR)$(HPS)320x480a.$(EXEEXT)
320X600A_EXE =     $(SUBDIR)$(HPS)320x600a.$(EXEEXT)
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
	
exe: $(320X200A_EXE) $(320X200B_EXE) $(320X240A_EXE) $(320X350A_EXE) $(320X400A_EXE) $(320X480A_EXE) $(320X600A_EXE) .symbolic

!ifdef 320X200A_EXE
$(320X200A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x200a.obj
	%write tmp.cmd option quiet option map=$(320X200A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x200a.obj name $(320X200A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X200B_EXE
$(320X200B_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x200b.obj
	%write tmp.cmd option quiet option map=$(320X200B_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x200b.obj name $(320X200B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X240A_EXE
$(320X240A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x240a.obj
	%write tmp.cmd option quiet option map=$(320X240A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x240a.obj name $(320X240A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)240vga.bmp $(SUBDIR)$(HPS)240l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X350A_EXE
$(320X350A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x350a.obj
	%write tmp.cmd option quiet option map=$(320X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x350a.obj name $(320X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)350vga.bmp $(SUBDIR)$(HPS)350l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X400A_EXE
$(320X400A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x400a.obj
	%write tmp.cmd option quiet option map=$(320X400A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x400a.obj name $(320X400A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)400vga.bmp $(SUBDIR)$(HPS)400l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X480A_EXE
$(320X480A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x480a.obj
	%write tmp.cmd option quiet option map=$(320X480A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x480a.obj name $(320X480A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)480vga.bmp $(SUBDIR)$(HPS)480l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X600A_EXE
$(320X600A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x600a.obj
	%write tmp.cmd option quiet option map=$(320X600A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x600a.obj name $(320X600A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)600vga.bmp $(SUBDIR)$(HPS)600l8v.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

