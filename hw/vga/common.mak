# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i=.. -i..$(HPS)..
NOW_BUILDING = HW_VGA_LIB

TEST_EXE =     $(SUBDIR)$(HPS)test.exe

!ifeq TARGET_MSDOS 16
!ifndef TARGET_WINDOWS
VGA240_EXE =   $(SUBDIR)$(HPS)vga240.exe
DRAWVRL3_EXE = $(SUBDIR)$(HPS)drawvrl3.exe
!endif
!endif

!ifndef TARGET_WINDOWS
TGFX_EXE =     $(SUBDIR)$(HPS)tgfx.exe
DRAWVRL_EXE =  $(SUBDIR)$(HPS)drawvrl.exe
! ifeq MMODE l
PCX2VRL_EXE =  $(SUBDIR)$(HPS)pcx2vrl.exe
VRL2VRS_EXE =  $(SUBDIR)$(HPS)vrl2vrs.exe
PCXSSCUT_EXE = $(SUBDIR)$(HPS)pcxsscut.exe
! endif
! ifeq MMODE f
PCX2VRL_EXE =  $(SUBDIR)$(HPS)pcx2vrl.exe
VRL2VRS_EXE =  $(SUBDIR)$(HPS)vrl2vrs.exe
PCXSSCUT_EXE = $(SUBDIR)$(HPS)pcxsscut.exe
! endif
DRAWVRL2_EXE = $(SUBDIR)$(HPS)drawvrl2.exe
TMODESET_EXE = $(SUBDIR)$(HPS)tmodeset.exe
TMOTSENG_EXE = $(SUBDIR)$(HPS)tmotseng.exe
DRAWVRL4_EXE = $(SUBDIR)$(HPS)drawvrl4.exe
DRAWVRL5_EXE = $(SUBDIR)$(HPS)drawvrl5.exe
!endif

$(HW_VGA_LIB): $(SUBDIR)$(HPS)vga.obj $(SUBDIR)$(HPS)herc.obj $(SUBDIR)$(HPS)tseng.obj $(SUBDIR)$(HPS)vgach3c0.obj $(SUBDIR)$(HPS)vgastget.obj $(SUBDIR)$(HPS)vgatxt50.obj $(SUBDIR)$(HPS)vgaclks.obj $(SUBDIR)$(HPS)vgabicur.obj $(SUBDIR)$(HPS)vgasetmm.obj $(SUBDIR)$(HPS)vgarcrtc.obj $(SUBDIR)$(HPS)vgasemo.obj $(SUBDIR)$(HPS)vgaseco.obj $(SUBDIR)$(HPS)vgacrtcc.obj $(SUBDIR)$(HPS)vgacrtcr.obj $(SUBDIR)$(HPS)vgacrtcs.obj $(SUBDIR)$(HPS)vgasplit.obj $(SUBDIR)$(HPS)vgamodex.obj $(SUBDIR)$(HPS)vga9wide.obj $(SUBDIR)$(HPS)vgaalfpl.obj $(SUBDIR)$(HPS)vgaselcs.obj $(SUBDIR)$(HPS)vgastloc.obj $(SUBDIR)$(HPS)vrl1xlof.obj $(SUBDIR)$(HPS)vrl1xdrw.obj $(SUBDIR)$(HPS)vrl1ydrw.obj $(SUBDIR)$(HPS)vrl1xdrs.obj $(SUBDIR)$(HPS)vgawm1bc.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vga.obj      -+$(SUBDIR)$(HPS)herc.obj     -+$(SUBDIR)$(HPS)tseng.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgach3c0.obj -+$(SUBDIR)$(HPS)vgastget.obj -+$(SUBDIR)$(HPS)vgatxt50.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgaclks.obj  -+$(SUBDIR)$(HPS)vgabicur.obj -+$(SUBDIR)$(HPS)vgasetmm.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgarcrtc.obj -+$(SUBDIR)$(HPS)vgasemo.obj  -+$(SUBDIR)$(HPS)vgaseco.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgacrtcc.obj -+$(SUBDIR)$(HPS)vgacrtcr.obj -+$(SUBDIR)$(HPS)vgacrtcs.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgasplit.obj -+$(SUBDIR)$(HPS)vgamodex.obj -+$(SUBDIR)$(HPS)vga9wide.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgaalfpl.obj -+$(SUBDIR)$(HPS)vgaselcs.obj -+$(SUBDIR)$(HPS)vgastloc.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vrl1xlof.obj -+$(SUBDIR)$(HPS)vrl1xdrw.obj -+$(SUBDIR)$(HPS)vrl1ydrw.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vrl1xdrs.obj -+$(SUBDIR)$(HPS)vgawm1bc.obj

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
	
exe: $(TEST_EXE) $(TMODESET_EXE) $(TMOTSENG_EXE) $(PCX2VRL_EXE) $(VRL2VRS_EXE) $(PCXSSCUT_EXE) $(DRAWVRL_EXE) $(DRAWVRL2_EXE) $(DRAWVRL3_EXE) $(DRAWVRL4_EXE) $(DRAWVRL5_EXE) $(TGFX_EXE) $(VGA240_EXE) .symbolic

$(TEST_EXE): $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe

!ifdef TGFX_EXE
$(TGFX_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_VGAGFX_LIB) $(HW_VGAGFX_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tgfx.obj
	%write tmp.cmd option quiet option map=$(TGFX_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_VGAGFX_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tgfx.obj name $(TGFX_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
	@$(COPY) ed2.pcx $(SUBDIR)$(HPS)ed2.pcx
	@$(COPY) ed2.vrl $(SUBDIR)$(HPS)ed2.vrl
	@$(COPY) ed2.pal $(SUBDIR)$(HPS)ed2.pal
	@$(COPY) megaman.pcx $(SUBDIR)$(HPS)megaman.pcx
	@$(COPY) megaman.vrl $(SUBDIR)$(HPS)megaman.vrl
	@$(COPY) megaman.pal $(SUBDIR)$(HPS)megaman.pal
	@$(COPY) chikyuu.pcx $(SUBDIR)$(HPS)chikyuu.pcx
	@$(COPY) chikyuu.vrl $(SUBDIR)$(HPS)chikyuu.vrl
	@$(COPY) chikyuu.pal $(SUBDIR)$(HPS)chikyuu.pal
	@$(COPY) prussia.pcx $(SUBDIR)$(HPS)prussia.pcx
	@$(COPY) prussia.vrl $(SUBDIR)$(HPS)prussia.vrl
	@$(COPY) prussia.pal $(SUBDIR)$(HPS)prussia.pal
	@$(COPY) 46113319.pcx $(SUBDIR)$(HPS)46113319.pcx
	@$(COPY) 46113319.vrl $(SUBDIR)$(HPS)46113319.vrl
	@$(COPY) 46113319.pal $(SUBDIR)$(HPS)46113319.pal
!endif

!ifdef TMODESET_EXE
$(TMODESET_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tmodeset.obj
	%write tmp.cmd option quiet option map=$(TMODESET_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tmodeset.obj name $(TMODESET_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TMOTSENG_EXE
$(TMOTSENG_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tmotseng.obj $(SUBDIR)$(HPS)tmodeset.obj
	%write tmp.cmd option quiet option map=$(TMOTSENG_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tmotseng.obj name $(TMOTSENG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VGA240_EXE
$(VGA240_EXE): $(SUBDIR)$(HPS)vga240.obj
	%write tmp.cmd option quiet option map=$(VGA240_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)vga240.obj name $(VGA240_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef PCX2VRL_EXE
$(PCX2VRL_EXE): $(SUBDIR)$(HPS)pcx2vrl.obj
	%write tmp.cmd option quiet option map=$(PCX2VRL_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)pcx2vrl.obj name $(PCX2VRL_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VRL2VRS_EXE
$(VRL2VRS_EXE): $(SUBDIR)$(HPS)vrl2vrs.obj $(SUBDIR)$(HPS)comshtps.obj
	%write tmp.cmd option quiet option map=$(VRL2VRS_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)vrl2vrs.obj file $(SUBDIR)$(HPS)comshtps.obj name $(VRL2VRS_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DRAWVRL_EXE
$(DRAWVRL_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)drawvrl.obj
	%write tmp.cmd option quiet option map=$(DRAWVRL_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)drawvrl.obj name $(DRAWVRL_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DRAWVRL2_EXE
$(DRAWVRL2_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)drawvrl2.obj
	%write tmp.cmd option quiet option map=$(DRAWVRL2_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)drawvrl2.obj name $(DRAWVRL2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DRAWVRL3_EXE
$(DRAWVRL3_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)drawvrl3.obj
	%write tmp.cmd option quiet option map=$(DRAWVRL3_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)drawvrl3.obj name $(DRAWVRL3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DRAWVRL4_EXE
$(DRAWVRL4_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)drawvrl4.obj
	%write tmp.cmd option quiet option map=$(DRAWVRL4_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)drawvrl4.obj name $(DRAWVRL4_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef DRAWVRL5_EXE
$(DRAWVRL5_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)drawvrl5.obj
	%write tmp.cmd option quiet option map=$(DRAWVRL5_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)drawvrl5.obj name $(DRAWVRL5_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef PCXSSCUT_EXE
$(PCXSSCUT_EXE): $(SUBDIR)$(HPS)pcxsscut.obj $(SUBDIR)$(HPS)comshtps.obj
	%write tmp.cmd option quiet option map=$(PCXSSCUT_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)pcxsscut.obj file $(SUBDIR)$(HPS)comshtps.obj name $(PCXSSCUT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

