# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NOW_BUILDING = HW_VGA_LIB

TEST_EXE =     $(SUBDIR)$(HPS)test.exe

!ifeq TARGET_MSDOS 16
!ifndef TARGET_WINDOWS
VGA240_EXE =   $(SUBDIR)$(HPS)vga240.exe
!endif
!endif

!ifndef TARGET_WINDOWS
TGFX_EXE =     $(SUBDIR)$(HPS)tgfx.exe
TMODESET_EXE = $(SUBDIR)$(HPS)tmodeset.exe
TMOTSENG_EXE = $(SUBDIR)$(HPS)tmotseng.exe
!endif

$(HW_VGA_LIB): $(SUBDIR)$(HPS)vga.obj $(SUBDIR)$(HPS)herc.obj $(SUBDIR)$(HPS)tseng.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vga.obj -+$(SUBDIR)$(HPS)herc.obj -+$(SUBDIR)$(HPS)tseng.obj

$(HW_VGATTY_LIB): $(SUBDIR)$(HPS)vgatty.obj $(HW_VGA_LIB)
	wlib -q -b -c $(HW_VGATTY_LIB) -+$(SUBDIR)$(HPS)vgatty.obj

$(HW_VGAGUI_LIB): $(SUBDIR)$(HPS)vgagui.obj $(HW_VGA_LIB)
	wlib -q -b -c $(HW_VGAGUI_LIB) -+$(SUBDIR)$(HPS)vgagui.obj

$(HW_VGAGFX_LIB): $(SUBDIR)$(HPS)vgagfx.obj $(SUBDIR)$(HPS)gvg256.obj $(SUBDIR)$(HPS)tvg256.obj $(HW_VGATTY_LIB)
	wlib -q -b -c $(HW_VGAGFX_LIB) -+$(SUBDIR)$(HPS)vgagfx.obj -+$(SUBDIR)$(HPS)gvg256.obj -+$(SUBDIR)$(HPS)tvg256.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	$(CC) @tmp.cmd

all: lib exe
       
lib: $(HW_VGA_LIB) $(HW_VGATTY_LIB) $(HW_VGAGUI_LIB) $(HW_VGAGFX_LIB) .symbolic
	
exe: $(TEST_EXE) $(TMODESET_EXE) $(TMOTSENG_EXE) $(TGFX_EXE) $(VGA240_EXE) .symbolic

$(TEST_EXE): $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

!ifdef TGFX_EXE
$(TGFX_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_VGAGFX_LIB) $(HW_VGAGFX_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tgfx.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_VGAGFX_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tgfx.obj name $(TGFX_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TMODESET_EXE
$(TMODESET_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tmodeset.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tmodeset.obj name $(TMODESET_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TMOTSENG_EXE
$(TMOTSENG_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tmotseng.obj $(SUBDIR)$(HPS)tmodeset.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tmotseng.obj name $(TMOTSENG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VGA240_EXE
$(VGA240_EXE): $(SUBDIR)$(HPS)vga240.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)vga240.obj name $(VGA240_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

