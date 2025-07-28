# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -i"../../.." -i"../../../../fmt/bmp"
NOW_BUILDING = TESTBMP_TEST

!ifndef PC98
! ifndef TARGET_WINDOWS
320X240A_EXE =     $(SUBDIR)$(HPS)320x240a.$(EXEEXT)
320X240B_EXE =     $(SUBDIR)$(HPS)320x240b.$(EXEEXT)
320X300A_EXE =     $(SUBDIR)$(HPS)320x300a.$(EXEEXT)
320X300B_EXE =     $(SUBDIR)$(HPS)320x300b.$(EXEEXT)
320X350A_EXE =     $(SUBDIR)$(HPS)320x350a.$(EXEEXT)
320X350B_EXE =     $(SUBDIR)$(HPS)320x350b.$(EXEEXT)
320X400A_EXE =     $(SUBDIR)$(HPS)320x400a.$(EXEEXT)
320X400B_EXE =     $(SUBDIR)$(HPS)320x400b.$(EXEEXT)
320X480B_EXE =     $(SUBDIR)$(HPS)320x480b.$(EXEEXT)
320X600B_EXE =     $(SUBDIR)$(HPS)320x600b.$(EXEEXT)
360X400B_EXE =     $(SUBDIR)$(HPS)360x400b.$(EXEEXT)
360X480B_EXE =     $(SUBDIR)$(HPS)360x480b.$(EXEEXT)
360X600B_EXE =     $(SUBDIR)$(HPS)360x600b.$(EXEEXT)
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
       
lib: $(FMT_BMP_LIB) .symbolic
	
exe: $(320X240A_EXE) $(320X240B_EXE) $(320X300A_EXE) $(320X300B_EXE) $(320X350A_EXE) $(320X350B_EXE) $(320X400A_EXE) $(320X400B_EXE) $(320X480B_EXE) $(320X600B_EXE) $(360X400B_EXE) $(360X480B_EXE) $(360X600B_EXE) $(640X350A_EXE) $(640X400A_EXE) $(640X480A_EXE) $(640X480L_EXE) $(640X480M_EXE) $(800X600A_EXE) $(800X600L_EXE) $(800X600M_EXE) $(1024X768_EXE) $(1024768L_EXE) $(1024768M_EXE) $(1281024L_EXE) $(1281024M_EXE) .symbolic

!ifdef 320X240A_EXE
$(320X240A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x240a.obj
	%write tmp.cmd option quiet option map=$(320X240A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x240a.obj name $(320X240A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)240vga.bmp $(SUBDIR)$(HPS)320240_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X240B_EXE
$(320X240B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x240b.obj
	%write tmp.cmd option quiet option map=$(320X240B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x240b.obj name $(320X240B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)240vga.bmp $(SUBDIR)$(HPS)320240_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X300A_EXE
$(320X300A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x300a.obj
	%write tmp.cmd option quiet option map=$(320X300A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x300a.obj name $(320X300A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)300vga.bmp $(SUBDIR)$(HPS)320300_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X300B_EXE
$(320X300B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x300b.obj
	%write tmp.cmd option quiet option map=$(320X300B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x300b.obj name $(320X300B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)300vga.bmp $(SUBDIR)$(HPS)320300_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X350A_EXE
$(320X350A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x350a.obj
	%write tmp.cmd option quiet option map=$(320X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x350a.obj name $(320X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)350vga.bmp $(SUBDIR)$(HPS)320350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X350B_EXE
$(320X350B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x350b.obj
	%write tmp.cmd option quiet option map=$(320X350B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x350b.obj name $(320X350B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)350vga.bmp $(SUBDIR)$(HPS)320350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X400A_EXE
$(320X400A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x400a.obj
	%write tmp.cmd option quiet option map=$(320X400A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x400a.obj name $(320X400A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)400vga.bmp $(SUBDIR)$(HPS)320400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X400B_EXE
$(320X400B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x400b.obj
	%write tmp.cmd option quiet option map=$(320X400B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x400b.obj name $(320X400B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)400vga.bmp $(SUBDIR)$(HPS)320400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X480B_EXE
$(320X480B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x480b.obj
	%write tmp.cmd option quiet option map=$(320X480B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x480b.obj name $(320X480B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)480vga.bmp $(SUBDIR)$(HPS)320480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 320X600B_EXE
$(320X600B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)320x600b.obj
	%write tmp.cmd option quiet option map=$(320X600B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)320x600b.obj name $(320X600B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w320$(HPS)600vga.bmp $(SUBDIR)$(HPS)320600_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X400B_EXE
$(360X400B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)360x400b.obj
	%write tmp.cmd option quiet option map=$(360X400B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)360x400b.obj name $(360X400B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)400vga.bmp $(SUBDIR)$(HPS)360400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X480B_EXE
$(360X480B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)360x480b.obj
	%write tmp.cmd option quiet option map=$(360X480B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)360x480b.obj name $(360X480B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)480vga.bmp $(SUBDIR)$(HPS)360480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 360X600B_EXE
$(360X600B_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)360x600b.obj
	%write tmp.cmd option quiet option map=$(360X600B_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)360x600b.obj name $(360X600B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w360$(HPS)600vga.bmp $(SUBDIR)$(HPS)360600_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X350A_EXE
$(640X350A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x350a.obj
	%write tmp.cmd option quiet option map=$(640X350A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x350a.obj name $(640X350A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)350vga.bmp $(SUBDIR)$(HPS)640350_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X400A_EXE
$(640X400A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x400a.obj
	%write tmp.cmd option quiet option map=$(640X400A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x400a.obj name $(640X400A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)400vga.bmp $(SUBDIR)$(HPS)640400_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480A_EXE
$(640X480A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x480a.obj
	%write tmp.cmd option quiet option map=$(640X480A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x480a.obj name $(640X480A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480L_EXE
$(640X480L_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x480l.obj
	%write tmp.cmd option quiet option map=$(640X480L_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x480l.obj name $(640X480L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 640X480M_EXE
$(640X480M_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)640x480m.obj
	%write tmp.cmd option quiet option map=$(640X480M_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)640x480m.obj name $(640X480M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480vga.bmp $(SUBDIR)$(HPS)640480_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w640$(HPS)480m.bmp $(SUBDIR)$(HPS)640480_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600A_EXE
$(800X600A_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)800x600a.obj
	%write tmp.cmd option quiet option map=$(800X600A_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)800x600a.obj name $(800X600A_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_8.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600L_EXE
$(800X600L_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)800x600l.obj
	%write tmp.cmd option quiet option map=$(800X600L_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)800x600l.obj name $(800X600L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_4.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 800X600M_EXE
$(800X600M_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)800x600m.obj
	%write tmp.cmd option quiet option map=$(800X600M_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)800x600m.obj name $(800X600M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600vga.bmp $(SUBDIR)$(HPS)800600_1.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w800$(HPS)600m.bmp $(SUBDIR)$(HPS)800600_m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024768L_EXE
$(1024768L_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)1024768l.obj
	%write tmp.cmd option quiet option map=$(1024768L_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)1024768l.obj name $(1024768L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247684.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024768M_EXE
$(1024768M_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)1024768m.obj
	%write tmp.cmd option quiet option map=$(1024768M_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)1024768m.obj name $(1024768M_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247681.bmp
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)1bpp$(HPS)w1024$(HPS)768m.bmp $(SUBDIR)$(HPS)1024768m.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1024X768_EXE
$(1024X768_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)1024x768.obj
	%write tmp.cmd option quiet option map=$(1024X768_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)1024x768.obj name $(1024X768_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)256$(HPS)w1024$(HPS)768vga.bmp $(SUBDIR)$(HPS)10247688.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1281024L_EXE
$(1281024L_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)1281024l.obj
	%write tmp.cmd option quiet option map=$(1281024L_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)1281024l.obj name $(1281024L_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)img$(HPS)4bpp$(HPS)w1280$(HPS)1024vga.bmp $(SUBDIR)$(HPS)12810244.bmp
	@$(COPY) ..$(HPS)..$(HPS)..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef 1281024M_EXE
$(1281024M_EXE): $(FMT_BMP_LIB) $(SUBDIR)$(HPS)1281024m.obj
	%write tmp.cmd option quiet option map=$(1281024M_EXE).map system $(WLINK_CON_SYSTEM) library $(FMT_BMP_LIB) file $(SUBDIR)$(HPS)1281024m.obj name $(1281024M_EXE)
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

