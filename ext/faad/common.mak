# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_FAAD_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)bits.obj &
    $(PRFX)cfft.obj &
    $(PRFX)common.obj &
    $(PRFX)decoder.obj &
    $(PRFX)drc.obj &
    $(PRFX)drm_dec.obj &
    $(PRFX)error.obj &
    $(PRFX)filtbank.obj &
    $(PRFX)hcr.obj &
    $(PRFX)huffman.obj &
    $(PRFX)ic_pred.obj &
    $(PRFX)is.obj &
    $(PRFX)lt_pred.obj &
    $(PRFX)mdct.obj &
    $(PRFX)mp4.obj &
    $(PRFX)ms.obj &
    $(PRFX)output.obj &
    $(PRFX)pns.obj &
    $(PRFX)ps_dec.obj &
    $(PRFX)pssyntax.obj &
    $(PRFX)pulse.obj &
    $(PRFX)rvlc.obj &
    $(PRFX)sbr_dct.obj &
    $(PRFX)sbr_dec.obj &
    $(PRFX)sbr_e_nf.obj &
    $(PRFX)sbr_fbt.obj &
    $(PRFX)sbrhfadj.obj &
    $(PRFX)sbrhfgen.obj &
    $(PRFX)sbr_huff.obj &
    $(PRFX)sbr_qmf.obj &
    $(PRFX)sbr_synt.obj &
    $(PRFX)sbr_tfgr.obj &
    $(PRFX)specrec.obj &
    $(PRFX)ssr.obj &
    $(PRFX)ssr_fb.obj &
    $(PRFX)ssr_ipqf.obj &
    $(PRFX)syntax.obj &
    $(PRFX)tns.obj &
    $(PRFX)aacaudio.obj

!ifdef EXT_FAAD_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_FAAD_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_FAAD_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

all: lib exe .symbolic
       
lib: $(EXT_FAAD_LIB) .symbolic

exe: .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

