# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_SPEEX_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)bits.obj &
    $(PRFX)buffer.obj &
    $(PRFX)cb_search.obj &
    $(PRFX)exc_10_16_table.obj &
    $(PRFX)exc_10_32_table.obj &
    $(PRFX)exc_20_32_table.obj &
    $(PRFX)exc_5_256_table.obj &
    $(PRFX)exc_5_64_table.obj &
    $(PRFX)exc_8_128_table.obj &
    $(PRFX)fftwrap.obj &
    $(PRFX)filterbank.obj &
    $(PRFX)filters.obj &
    $(PRFX)gain_table.obj &
    $(PRFX)gain_table_lbr.obj &
    $(PRFX)hexc_10_32_table.obj &
    $(PRFX)hexc_table.obj &
    $(PRFX)high_lsp_tables.obj &
    $(PRFX)jitter.obj &
    $(PRFX)kiss_fft.obj &
    $(PRFX)kiss_fftr.obj &
    $(PRFX)lpc.obj &
    $(PRFX)lsp.obj &
    $(PRFX)lsp_tables_nb.obj &
    $(PRFX)ltp.obj &
    $(PRFX)mdf.obj &
    $(PRFX)modes.obj &
    $(PRFX)modes_wb.obj &
    $(PRFX)nb_celp.obj &
    $(PRFX)preprocess.obj &
    $(PRFX)quant_lsp.obj &
    $(PRFX)resample.obj &
    $(PRFX)sb_celp.obj &
    $(PRFX)scal.obj &
    $(PRFX)smallft.obj &
    $(PRFX)speex.obj &
    $(PRFX)speex_callbacks.obj &
    $(PRFX)speex_header.obj &
    $(PRFX)stereo.obj &
    $(PRFX)testdenoise.obj &
    $(PRFX)testecho.obj &
    $(PRFX)testenc.obj &
    $(PRFX)testenc_uwb.obj &
    $(PRFX)testenc_wb.obj &
    $(PRFX)testjitter.obj &
    $(PRFX)vbr.obj &
    $(PRFX)vq.obj &
    $(PRFX)window.obj

all: lib exe .symbolic
       
!ifdef EXT_SPEEX_SPEEXDEC_EXE
SPEEXDEC_EXE_OBJS = &
    $(SUBDIR)$(HPS)speexdec.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj &
    $(SUBDIR)$(HPS)wav_io.obj

$(EXT_SPEEX_SPEEXDEC_EXE): $(EXT_LIBOGG_LIB) $(EXT_SPEEX_LIB) $(SPEEXDEC_EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(SPEEXDEC_EXE_OBJS)} $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) library $(EXT_SPEEX_LIB) name $(EXT_SPEEX_SPEEXDEC_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_SPEEX_SPEEXENC_EXE
SPEEXENC_EXE_OBJS = &
    $(SUBDIR)$(HPS)speexenc.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj &
    $(SUBDIR)$(HPS)wav_io.obj &
    $(SUBDIR)$(HPS)wave_out.obj &
    $(SUBDIR)$(HPS)skeleton.obj

$(EXT_SPEEX_SPEEXENC_EXE): $(EXT_LIBOGG_LIB) $(EXT_SPEEX_LIB) $(SPEEXENC_EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(SPEEXENC_EXE_OBJS)} $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) library $(EXT_SPEEX_LIB) name $(EXT_SPEEX_SPEEXENC_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_SPEEX_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_SPEEX_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_SPEEX_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_SPEEX_LIB) .symbolic

exe: $(EXT_SPEEX_SPEEXDEC_EXE) $(EXT_SPEEX_SPEEXENC_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

