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
640X350A_EXE =     $(SUBDIR)$(HPS)640x350a.$(EXEEXT)
640X400A_EXE =     $(SUBDIR)$(HPS)640x400a.$(EXEEXT)
640X480A_EXE =     $(SUBDIR)$(HPS)640x480a.$(EXEEXT)
640X480L_EXE =     $(SUBDIR)$(HPS)640x480l.$(EXEEXT)
640X480M_EXE =     $(SUBDIR)$(HPS)640x480m.$(EXEEXT)
800X600A_EXE =     $(SUBDIR)$(HPS)800x600a.$(EXEEXT)
800X600L_EXE =     $(SUBDIR)$(HPS)800x600l.$(EXEEXT)
800X600M_EXE =     $(SUBDIR)$(HPS)800x600m.$(EXEEXT)
1024X768_EXE =     $(SUBDIR)$(HPS)1024x768.$(EXEEXT)
1024768L_EXE =     $(SUBDIR)$(HPS)1024768l.$(EXEEXT)
1024768M_EXE =     $(SUBDIR)$(HPS)1024768m.$(EXEEXT)
1281024L_EXE =     $(SUBDIR)$(HPS)1281024l.$(EXEEXT)
1281024M_EXE =     $(SUBDIR)$(HPS)1281024m.$(EXEEXT)
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
	
exe: $(640X350A_EXE) $(640X400A_EXE) $(640X480A_EXE) $(640X480L_EXE) $(640X480M_EXE) $(800X600A_EXE) $(800X600L_EXE) $(800X600M_EXE) $(1024X768_EXE) $(1024768L_EXE) $(1024768M_EXE) $(1281024L_EXE) $(1281024M_EXE) .symbolic

!ifdef 640X350A_EXE
$(640X350A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)640x350a.obj
	%write tmp.cmd option quiet option map=$(640X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)640x350a.obj name $(640X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350_8.bmp
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

!ifdef 800X600A_EXE
$(800X600A_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)800x600a.obj
	%write tmp.cmd option quiet option map=$(800X600A_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)800x600a.obj name $(800X600A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600L_EXE
$(800X600L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)800x600l.obj
	%write tmp.cmd option quiet option map=$(800X600L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)800x600l.obj name $(800X600L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600M_EXE
$(800X600M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)800x600m.obj
	%write tmp.cmd option quiet option map=$(800X600M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)800x600m.obj name $(800X600M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600m.bmp $(SUBDIR)$(HPS)800600_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024768L_EXE
$(1024768L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)1024768l.obj
	%write tmp.cmd option quiet option map=$(1024768L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)1024768l.obj name $(1024768L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247684.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024768M_EXE
$(1024768M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)1024768m.obj
	%write tmp.cmd option quiet option map=$(1024768M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)1024768m.obj name $(1024768M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247681.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1024$(HPS)768m.bmp $(SUBDIR)$(HPS)1024768m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024X768_EXE
$(1024X768_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)1024x768.obj
	%write tmp.cmd option quiet option map=$(1024X768_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)1024x768.obj name $(1024X768_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247688.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1281024L_EXE
$(1281024L_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)1281024l.obj
	%write tmp.cmd option quiet option map=$(1281024L_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)1281024l.obj name $(1281024L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1280$(HPS)1024vga.bmp $(SUBDIR)$(HPS)12810244.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1281024M_EXE
$(1281024M_EXE): $(LIBBMP_LIB) $(SUBDIR)$(HPS)1281024m.obj
	%write tmp.cmd option quiet option map=$(1281024M_EXE).map system $(WLINK_CON_SYSTEM) library $(LIBBMP_LIB) file $(SUBDIR)$(HPS)1281024m.obj name $(1281024M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1280$(HPS)1024vga.bmp $(SUBDIR)$(HPS)12810241.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1280$(HPS)1024m.bmp $(SUBDIR)$(HPS)1281024m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

