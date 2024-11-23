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
320X300A_EXE =     $(SUBDIR)$(HPS)320x300a.$(EXEEXT)
320X350A_EXE =     $(SUBDIR)$(HPS)320x350a.$(EXEEXT)
320X400A_EXE =     $(SUBDIR)$(HPS)320x400a.$(EXEEXT)
320X480A_EXE =     $(SUBDIR)$(HPS)320x480a.$(EXEEXT)
320X600A_EXE =     $(SUBDIR)$(HPS)320x600a.$(EXEEXT)
360X200A_EXE =     $(SUBDIR)$(HPS)360x200a.$(EXEEXT)
360X240A_EXE =     $(SUBDIR)$(HPS)360x240a.$(EXEEXT)
360X300A_EXE =     $(SUBDIR)$(HPS)360x300a.$(EXEEXT)
360X350A_EXE =     $(SUBDIR)$(HPS)360x350a.$(EXEEXT)
360X400A_EXE =     $(SUBDIR)$(HPS)360x400a.$(EXEEXT)
360X480A_EXE =     $(SUBDIR)$(HPS)360x480a.$(EXEEXT)
360X600A_EXE =     $(SUBDIR)$(HPS)360x600a.$(EXEEXT)
320X200L_EXE =     $(SUBDIR)$(HPS)320x200l.$(EXEEXT)
320X200M_EXE =     $(SUBDIR)$(HPS)320x200m.$(EXEEXT)
640X200L_EXE =     $(SUBDIR)$(HPS)640x200l.$(EXEEXT)
640X200M_EXE =     $(SUBDIR)$(HPS)640x200m.$(EXEEXT)
640X240L_EXE =     $(SUBDIR)$(HPS)640x240l.$(EXEEXT)
640X240M_EXE =     $(SUBDIR)$(HPS)640x240m.$(EXEEXT)
640X350L_EXE =     $(SUBDIR)$(HPS)640x350l.$(EXEEXT)
640X350M_EXE =     $(SUBDIR)$(HPS)640x350m.$(EXEEXT)
640X400L_EXE =     $(SUBDIR)$(HPS)640x400l.$(EXEEXT)
640X400M_EXE =     $(SUBDIR)$(HPS)640x400m.$(EXEEXT)
640X480L_EXE =     $(SUBDIR)$(HPS)640x480l.$(EXEEXT)
640X480M_EXE =     $(SUBDIR)$(HPS)640x480m.$(EXEEXT)
800X600L_EXE =     $(SUBDIR)$(HPS)800x600l.$(EXEEXT)
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
	
exe: $(320X200A_EXE) $(320X200B_EXE) $(320X240A_EXE) $(320X300A_EXE) $(320X350A_EXE) $(320X400A_EXE) $(320X480A_EXE) $(320X600A_EXE) $(360X200A_EXE) $(360X240A_EXE) $(360X300A_EXE) $(360X350A_EXE) $(360X400A_EXE) $(360X480A_EXE) $(360X600A_EXE) $(320X200L_EXE) $(320X200M_EXE) $(640X200L_EXE) $(640X200M_EXE) $(640X240L_EXE) $(640X240M_EXE) $(640X350L_EXE) $(640X350M_EXE) $(640X400L_EXE) $(640X400M_EXE) $(640X480L_EXE) $(640X480M_EXE) $(800X600L_EXE) .symbolic

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

!ifdef 320X300A_EXE
$(320X300A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x300a.obj
	%write tmp.cmd option quiet option map=$(320X300A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x300a.obj name $(320X300A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)300vga.bmp $(SUBDIR)$(HPS)300l8v.bmp
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

!ifdef 360X200A_EXE
$(360X200A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x200a.obj
	%write tmp.cmd option quiet option map=$(360X200A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x200a.obj name $(360X200A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X240A_EXE
$(360X240A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x240a.obj
	%write tmp.cmd option quiet option map=$(360X240A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x240a.obj name $(360X240A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)240vga.bmp $(SUBDIR)$(HPS)240l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X300A_EXE
$(360X300A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x300a.obj
	%write tmp.cmd option quiet option map=$(360X300A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x300a.obj name $(360X300A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)300vga.bmp $(SUBDIR)$(HPS)300l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X350A_EXE
$(360X350A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x350a.obj
	%write tmp.cmd option quiet option map=$(360X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x350a.obj name $(360X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)350vga.bmp $(SUBDIR)$(HPS)350l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X400A_EXE
$(360X400A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x400a.obj
	%write tmp.cmd option quiet option map=$(360X400A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x400a.obj name $(360X400A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)400vga.bmp $(SUBDIR)$(HPS)400l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X480A_EXE
$(360X480A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x480a.obj
	%write tmp.cmd option quiet option map=$(360X480A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x480a.obj name $(360X480A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)480vga.bmp $(SUBDIR)$(HPS)480l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X600A_EXE
$(360X600A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)360x600a.obj
	%write tmp.cmd option quiet option map=$(360X600A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)360x600a.obj name $(360X600A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)600vga.bmp $(SUBDIR)$(HPS)600l8v6.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X200L_EXE
$(320X200L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x200l.obj
	%write tmp.cmd option quiet option map=$(320X200L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x200l.obj name $(320X200L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X200M_EXE
$(320X200M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)320x200m.obj
	%write tmp.cmd option quiet option map=$(320X200M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)320x200m.obj name $(320X200M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w320$(HPS)200vga.bmp $(SUBDIR)$(HPS)200mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w320$(HPS)200m.bmp $(SUBDIR)$(HPS)200m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X200L_EXE
$(640X200L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x200l.obj
	%write tmp.cmd option quiet option map=$(640X200L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x200l.obj name $(640X200L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)200vga.bmp $(SUBDIR)$(HPS)200l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X200M_EXE
$(640X200M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x200m.obj
	%write tmp.cmd option quiet option map=$(640X200M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x200m.obj name $(640X200M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)200vga.bmp $(SUBDIR)$(HPS)200mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)200m.bmp $(SUBDIR)$(HPS)200m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X240L_EXE
$(640X240L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x240l.obj
	%write tmp.cmd option quiet option map=$(640X240L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x240l.obj name $(640X240L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)240vga.bmp $(SUBDIR)$(HPS)240l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X240M_EXE
$(640X240M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x240m.obj
	%write tmp.cmd option quiet option map=$(640X240M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x240m.obj name $(640X240M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)240vga.bmp $(SUBDIR)$(HPS)240mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)240m.bmp $(SUBDIR)$(HPS)240m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X350L_EXE
$(640X350L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x350l.obj
	%write tmp.cmd option quiet option map=$(640X350L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x350l.obj name $(640X350L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)350l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X350M_EXE
$(640X350M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x350m.obj
	%write tmp.cmd option quiet option map=$(640X350M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x350m.obj name $(640X350M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)350mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)350m.bmp $(SUBDIR)$(HPS)350m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X400L_EXE
$(640X400L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x400l.obj
	%write tmp.cmd option quiet option map=$(640X400L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x400l.obj name $(640X400L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)400l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X400M_EXE
$(640X400M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x400m.obj
	%write tmp.cmd option quiet option map=$(640X400M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x400m.obj name $(640X400M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)400mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)400m.bmp $(SUBDIR)$(HPS)400m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480L_EXE
$(640X480L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480l.obj
	%write tmp.cmd option quiet option map=$(640X480L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480l.obj name $(640X480L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)480l16.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480M_EXE
$(640X480M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x480m.obj
	%write tmp.cmd option quiet option map=$(640X480M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x480m.obj name $(640X480M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)480mc.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480m.bmp $(SUBDIR)$(HPS)480m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600L_EXE
$(800X600L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)800x600l.obj
	%write tmp.cmd option quiet option map=$(800X600L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)800x600l.obj name $(800X600L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)80060016.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

