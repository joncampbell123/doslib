# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_FAAD_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)bits.obj $(SUBDIR)$(HPS)cfft.obj $(SUBDIR)$(HPS)common.obj $(SUBDIR)$(HPS)decoder.obj $(SUBDIR)$(HPS)drc.obj $(SUBDIR)$(HPS)drm_dec.obj $(SUBDIR)$(HPS)error.obj $(SUBDIR)$(HPS)filtbank.obj $(SUBDIR)$(HPS)hcr.obj $(SUBDIR)$(HPS)huffman.obj $(SUBDIR)$(HPS)ic_pred.obj $(SUBDIR)$(HPS)is.obj $(SUBDIR)$(HPS)lt_pred.obj $(SUBDIR)$(HPS)mdct.obj $(SUBDIR)$(HPS)mp4.obj $(SUBDIR)$(HPS)ms.obj $(SUBDIR)$(HPS)output.obj $(SUBDIR)$(HPS)pns.obj $(SUBDIR)$(HPS)ps_dec.obj $(SUBDIR)$(HPS)pssyntax.obj $(SUBDIR)$(HPS)pulse.obj $(SUBDIR)$(HPS)rvlc.obj $(SUBDIR)$(HPS)sbr_dct.obj $(SUBDIR)$(HPS)sbr_dec.obj $(SUBDIR)$(HPS)sbr_e_nf.obj $(SUBDIR)$(HPS)sbr_fbt.obj $(SUBDIR)$(HPS)sbrhfadj.obj $(SUBDIR)$(HPS)sbrhfgen.obj $(SUBDIR)$(HPS)sbr_huff.obj $(SUBDIR)$(HPS)sbr_qmf.obj $(SUBDIR)$(HPS)sbr_synt.obj $(SUBDIR)$(HPS)sbr_tfgr.obj $(SUBDIR)$(HPS)specrec.obj $(SUBDIR)$(HPS)ssr.obj $(SUBDIR)$(HPS)ssr_fb.obj $(SUBDIR)$(HPS)ssr_ipqf.obj $(SUBDIR)$(HPS)syntax.obj $(SUBDIR)$(HPS)tns.obj $(SUBDIR)$(HPS)aacaudio.obj

!ifdef EXT_FAAD_LIB
$(EXT_FAAD_LIB): $(OBJS)
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)bits.obj -+$(SUBDIR)$(HPS)cfft.obj
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)common.obj -+$(SUBDIR)$(HPS)decoder.obj
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)drc.obj -+$(SUBDIR)$(HPS)drm_dec.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)error.obj -+$(SUBDIR)$(HPS)filtbank.obj
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)hcr.obj -+$(SUBDIR)$(HPS)huffman.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)ic_pred.obj -+$(SUBDIR)$(HPS)is.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)lt_pred.obj -+$(SUBDIR)$(HPS)mdct.obj
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)mp4.obj -+$(SUBDIR)$(HPS)ms.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)output.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)pns.obj -+$(SUBDIR)$(HPS)ps_dec.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)pssyntax.obj -+$(SUBDIR)$(HPS)pulse.obj
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)rvlc.obj -+$(SUBDIR)$(HPS)sbr_dct.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)sbr_dec.obj -+$(SUBDIR)$(HPS)sbr_e_nf.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)sbr_fbt.obj -+$(SUBDIR)$(HPS)sbrhfadj.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)sbrhfgen.obj -+$(SUBDIR)$(HPS)sbr_huff.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)sbr_qmf.obj -+$(SUBDIR)$(HPS)sbr_synt.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)sbr_tfgr.obj -+$(SUBDIR)$(HPS)specrec.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)ssr.obj -+$(SUBDIR)$(HPS)ssr_fb.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)ssr_ipqf.obj -+$(SUBDIR)$(HPS)syntax.obj 
	wlib -q -b -c $(EXT_FAAD_LIB) -+$(SUBDIR)$(HPS)tns.obj -+$(SUBDIR)$(HPS)aacaudio.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_FAAD_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

