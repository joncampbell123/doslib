# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = HW_VGA_LIB

!ifndef PC98
TEST_EXE =     $(SUBDIR)$(HPS)test.$(EXEEXT)
!endif

!ifndef PC98
!ifeq TARGET_MSDOS 16
!ifndef TARGET_WINDOWS
VGA240_EXE =   $(SUBDIR)$(HPS)vga240.$(EXEEXT)
DRAWVRL3_EXE = $(SUBDIR)$(HPS)drawvrl3.$(EXEEXT)
!endif
!endif
!endif

!ifndef TARGET_WINDOWS
! ifndef PC98
TGFX_EXE =     $(SUBDIR)$(HPS)tgfx.$(EXEEXT)
CGAFX1_EXE =   $(SUBDIR)$(HPS)cgafx1.$(EXEEXT)
CGAFX2_EXE =   $(SUBDIR)$(HPS)cgafx2.$(EXEEXT)
CGAFX3_EXE =   $(SUBDIR)$(HPS)cgafx3.$(EXEEXT)
CGAFX4_EXE =   $(SUBDIR)$(HPS)cgafx4.$(EXEEXT)
CGAFX4B_EXE =  $(SUBDIR)$(HPS)cgafx4b.$(EXEEXT)
CGAFX4C_EXE =  $(SUBDIR)$(HPS)cgafx4c.$(EXEEXT)
CGAFX5_EXE =   $(SUBDIR)$(HPS)cgafx5.$(EXEEXT)
CGAFX6_EXE =   $(SUBDIR)$(HPS)cgafx6.$(EXEEXT)
CGAFX6B_EXE =  $(SUBDIR)$(HPS)cgafx6b.$(EXEEXT)
CGAFX6C_EXE =  $(SUBDIR)$(HPS)cgafx6c.$(EXEEXT)
DRAWVRL_EXE =  $(SUBDIR)$(HPS)drawvrl.$(EXEEXT)
FONTEDIT_EXE = $(SUBDIR)$(HPS)fontedit.$(EXEEXT)
FONTLOAD_EXE = $(SUBDIR)$(HPS)fontload.$(EXEEXT)
FONTSAVE_EXE = $(SUBDIR)$(HPS)fontsave.$(EXEEXT)
!  ifeq MMODE l
VRLDBG_EXE =   $(SUBDIR)$(HPS)vrldbg.$(EXEEXT)
PCX2VRL_EXE =  $(SUBDIR)$(HPS)pcx2vrl.$(EXEEXT)
VRL2VRS_EXE =  $(SUBDIR)$(HPS)vrl2vrs.$(EXEEXT)
VRSDUMP_EXE =  $(SUBDIR)$(HPS)vrsdump.$(EXEEXT)
PCXSSCUT_EXE = $(SUBDIR)$(HPS)pcxsscut.$(EXEEXT)
MCGACAPM_EXE = $(SUBDIR)$(HPS)mcgacapm.$(EXEEXT)
!  endif
!  ifeq MMODE f
VRLDBG_EXE =   $(SUBDIR)$(HPS)vrldbg.$(EXEEXT)
PCX2VRL_EXE =  $(SUBDIR)$(HPS)pcx2vrl.$(EXEEXT)
VRL2VRS_EXE =  $(SUBDIR)$(HPS)vrl2vrs.$(EXEEXT)
VRSDUMP_EXE =  $(SUBDIR)$(HPS)vrsdump.$(EXEEXT)
PCXSSCUT_EXE = $(SUBDIR)$(HPS)pcxsscut.$(EXEEXT)
!  endif
DRAWVRL2_EXE = $(SUBDIR)$(HPS)drawvrl2.$(EXEEXT)
TMODESET_EXE = $(SUBDIR)$(HPS)tmodeset.$(EXEEXT)
TMOTSENG_EXE = $(SUBDIR)$(HPS)tmotseng.$(EXEEXT)
DRAWVRL4_EXE = $(SUBDIR)$(HPS)drawvrl4.$(EXEEXT)
DRAWVRL5_EXE = $(SUBDIR)$(HPS)drawvrl5.$(EXEEXT)
MCGACAPM_EXE = $(SUBDIR)$(HPS)mcgacapm.$(EXEEXT)
! endif
!endif

$(HW_VGA_LIB): $(SUBDIR)$(HPS)vga.obj $(SUBDIR)$(HPS)herc.obj $(SUBDIR)$(HPS)tseng.obj $(SUBDIR)$(HPS)vgach3c0.obj $(SUBDIR)$(HPS)vgastget.obj $(SUBDIR)$(HPS)vgatxt50.obj $(SUBDIR)$(HPS)vgaclks.obj $(SUBDIR)$(HPS)vgabicur.obj $(SUBDIR)$(HPS)vgasetmm.obj $(SUBDIR)$(HPS)vgarcrtc.obj $(SUBDIR)$(HPS)vgasemo.obj $(SUBDIR)$(HPS)vgaseco.obj $(SUBDIR)$(HPS)vgacrtcc.obj $(SUBDIR)$(HPS)vgacrtcr.obj $(SUBDIR)$(HPS)vgacrtcs.obj $(SUBDIR)$(HPS)vgasplit.obj $(SUBDIR)$(HPS)vgamodex.obj $(SUBDIR)$(HPS)vga9wide.obj $(SUBDIR)$(HPS)vgaalfpl.obj $(SUBDIR)$(HPS)vgaselcs.obj $(SUBDIR)$(HPS)vgastloc.obj $(SUBDIR)$(HPS)vrl1xlof.obj $(SUBDIR)$(HPS)vrl1xdrw.obj $(SUBDIR)$(HPS)vrl1ydrw.obj $(SUBDIR)$(HPS)vrl1xdrs.obj $(SUBDIR)$(HPS)vgawm1bc.obj $(SUBDIR)$(HPS)pcjrmem.obj $(SUBDIR)$(HPS)vgattyp8.obj $(SUBDIR)$(HPS)vgattyg.obj $(SUBDIR)$(HPS)vgattyj.obj $(SUBDIR)$(HPS)vrlrbasp.obj $(SUBDIR)$(HPS)vgapalb.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vga.obj      -+$(SUBDIR)$(HPS)herc.obj     -+$(SUBDIR)$(HPS)tseng.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgach3c0.obj -+$(SUBDIR)$(HPS)vgastget.obj -+$(SUBDIR)$(HPS)vgatxt50.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgaclks.obj  -+$(SUBDIR)$(HPS)vgabicur.obj -+$(SUBDIR)$(HPS)vgasetmm.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgarcrtc.obj -+$(SUBDIR)$(HPS)vgasemo.obj  -+$(SUBDIR)$(HPS)vgaseco.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgacrtcc.obj -+$(SUBDIR)$(HPS)vgacrtcr.obj -+$(SUBDIR)$(HPS)vgacrtcs.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgasplit.obj -+$(SUBDIR)$(HPS)vgamodex.obj -+$(SUBDIR)$(HPS)vga9wide.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgaalfpl.obj -+$(SUBDIR)$(HPS)vgaselcs.obj -+$(SUBDIR)$(HPS)vgastloc.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vrl1xlof.obj -+$(SUBDIR)$(HPS)vrl1xdrw.obj -+$(SUBDIR)$(HPS)vrl1ydrw.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vrl1xdrs.obj -+$(SUBDIR)$(HPS)vgawm1bc.obj -+$(SUBDIR)$(HPS)pcjrmem.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vgattyp8.obj -+$(SUBDIR)$(HPS)vgattyg.obj  -+$(SUBDIR)$(HPS)vgattyj.obj
	wlib -q -b -c $(HW_VGA_LIB) -+$(SUBDIR)$(HPS)vrlrbasp.obj -+$(SUBDIR)$(HPS)vgapalb.obj

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
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

# VGA240 needs -zu because of interrupt handler
$(SUBDIR)$(HPS)vga240.obj: vga240.c
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) -zu $[@
	$(CC) @tmp.cmd
!ifdef TINYMODE
	$(OMFSEGDG) -i $@ -o $@
!endif

all: $(OMFSEGDG) lib exe
       
lib: $(HW_VGA_LIB) $(HW_VGATTY_LIB) $(HW_VGAGUI_LIB) $(HW_VGAGFX_LIB) .symbolic
	
exe: $(TEST_EXE) $(TMODESET_EXE) $(TMOTSENG_EXE) $(PCX2VRL_EXE) $(VRLDBG_EXE) $(VRL2VRS_EXE) $(PCXSSCUT_EXE) $(DRAWVRL_EXE) $(VRSDUMP_EXE) $(DRAWVRL2_EXE) $(DRAWVRL3_EXE) $(DRAWVRL4_EXE) $(DRAWVRL5_EXE) $(TGFX_EXE) $(VGA240_EXE) $(CGAFX1_EXE) $(CGAFX2_EXE) $(CGAFX3_EXE) $(CGAFX4_EXE) $(CGAFX4B_EXE) $(CGAFX4C_EXE) $(CGAFX5_EXE) $(CGAFX6_EXE) $(CGAFX6B_EXE) $(CGAFX6C_EXE) $(FONTEDIT_EXE) $(FONTLOAD_EXE) $(FONTSAVE_EXE) $(MCGACAPM_EXE) .symbolic

!ifdef TEST_EXE
$(TEST_EXE): $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)test.obj
	%write tmp.cmd option quiet option map=$(TEST_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGATTY_LIB_WLINK_LIBRARIES) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)test.obj name $(TEST_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX1_EXE
$(CGAFX1_EXE): $(SUBDIR)$(HPS)cgafx1.obj
	%write tmp.cmd option quiet option map=$(CGAFX1_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx1.obj name $(CGAFX1_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX2_EXE
$(CGAFX2_EXE): $(SUBDIR)$(HPS)cgafx2.obj
	%write tmp.cmd option quiet option map=$(CGAFX2_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx2.obj name $(CGAFX2_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX3_EXE
$(CGAFX3_EXE): $(SUBDIR)$(HPS)cgafx3.obj
	%write tmp.cmd option quiet option map=$(CGAFX3_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx3.obj name $(CGAFX3_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX4_EXE
$(CGAFX4_EXE): $(SUBDIR)$(HPS)cgafx4.obj
	%write tmp.cmd option quiet option map=$(CGAFX4_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx4.obj name $(CGAFX4_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX4B_EXE
$(CGAFX4B_EXE): $(SUBDIR)$(HPS)cgafx4b.obj $(SUBDIR)$(HPS)cgafx4.obj
	%write tmp.cmd option quiet option map=$(CGAFX4B_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx4b.obj name $(CGAFX4B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX4C_EXE
$(CGAFX4C_EXE): $(SUBDIR)$(HPS)cgafx4c.obj $(SUBDIR)$(HPS)cgafx4.obj
	%write tmp.cmd option quiet option map=$(CGAFX4C_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx4c.obj name $(CGAFX4C_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX5_EXE
$(CGAFX5_EXE): $(SUBDIR)$(HPS)cgafx5.obj
	%write tmp.cmd option quiet option map=$(CGAFX5_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx5.obj name $(CGAFX5_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX6_EXE
$(CGAFX6_EXE): $(SUBDIR)$(HPS)cgafx6.obj
	%write tmp.cmd option quiet option map=$(CGAFX6_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx6.obj name $(CGAFX6_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX6B_EXE
$(CGAFX6B_EXE): $(SUBDIR)$(HPS)cgafx6b.obj $(SUBDIR)$(HPS)cgafx6.obj
	%write tmp.cmd option quiet option map=$(CGAFX6B_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx6b.obj name $(CGAFX6B_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CGAFX6C_EXE
$(CGAFX6C_EXE): $(SUBDIR)$(HPS)cgafx6c.obj $(SUBDIR)$(HPS)cgafx6.obj
	%write tmp.cmd option quiet option map=$(CGAFX6C_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)cgafx6c.obj name $(CGAFX6C_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

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
	@$(COPY) sorc1.png $(SUBDIR)$(HPS)sorc1.png
	@$(COPY) sorc1.vrl $(SUBDIR)$(HPS)sorc1.vrl
	@$(COPY) sorc1.pal $(SUBDIR)$(HPS)sorc1.pal
	@$(COPY) sorc2.png $(SUBDIR)$(HPS)sorc2.png
	@$(COPY) sorc2.vrl $(SUBDIR)$(HPS)sorc2.vrl
	@$(COPY) sorc2.pal $(SUBDIR)$(HPS)sorc2.pal
!endif

!ifdef TMODESET_EXE
$(TMODESET_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)tmodeset.obj
	%write tmp.cmd option quiet option map=$(TMODESET_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)tmodeset.obj name $(TMODESET_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef MCGACAPM_EXE
$(MCGACAPM_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)mcgacapm.obj
	%write tmp.cmd option quiet option map=$(MCGACAPM_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)mcgacapm.obj name $(MCGACAPM_EXE)
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

!ifdef VRLDBG_EXE
$(VRLDBG_EXE): $(SUBDIR)$(HPS)vrldbg.obj
	%write tmp.cmd option quiet option map=$(VRLDBG_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)vrldbg.obj name $(VRLDBG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VRSDUMP_EXE
$(VRSDUMP_EXE): $(SUBDIR)$(HPS)vrsdump.obj
	%write tmp.cmd option quiet option map=$(VRSDUMP_EXE).map system $(WLINK_CON_SYSTEM) file $(SUBDIR)$(HPS)vrsdump.obj name $(VRSDUMP_EXE)
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

!ifdef FONTEDIT_EXE
$(FONTEDIT_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(HW_VGATTY_LIB) $(HW_VGATTY_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)fontedit.obj
	%write tmp.cmd option quiet option map=$(FONTEDIT_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) $(HW_VGATTY_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)fontedit.obj name $(FONTEDIT_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
	# example VGA fonts
	@$(COPY) *.vga $(SUBDIR)$(HPS)
	@$(COPY) fontedit.txt $(SUBDIR)$(HPS)
!endif

!ifdef FONTLOAD_EXE
$(FONTLOAD_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)fontload.obj
	%write tmp.cmd option quiet option map=$(FONTLOAD_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)fontload.obj name $(FONTLOAD_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef FONTSAVE_EXE
$(FONTSAVE_EXE): $(HW_VGA_LIB) $(HW_VGA_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)fontsave.obj
	%write tmp.cmd option quiet option map=$(FONTSAVE_EXE).map system $(WLINK_CON_SYSTEM) $(HW_VGA_LIB_WLINK_LIBRARIES) file $(SUBDIR)$(HPS)fontsave.obj name $(FONTSAVE_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(HW_VGA_LIB)
          del tmp.cmd
          @echo Cleaning done

