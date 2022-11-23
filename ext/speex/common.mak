# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_SPEEX_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)bits.obj $(SUBDIR)$(HPS)buffer.obj $(SUBDIR)$(HPS)cb_search.obj $(SUBDIR)$(HPS)exc_10_16_table.obj $(SUBDIR)$(HPS)exc_10_32_table.obj $(SUBDIR)$(HPS)exc_20_32_table.obj $(SUBDIR)$(HPS)exc_5_256_table.obj $(SUBDIR)$(HPS)exc_5_64_table.obj $(SUBDIR)$(HPS)exc_8_128_table.obj $(SUBDIR)$(HPS)fftwrap.obj $(SUBDIR)$(HPS)filterbank.obj $(SUBDIR)$(HPS)filters.obj $(SUBDIR)$(HPS)gain_table.obj $(SUBDIR)$(HPS)gain_table_lbr.obj $(SUBDIR)$(HPS)hexc_10_32_table.obj $(SUBDIR)$(HPS)hexc_table.obj $(SUBDIR)$(HPS)high_lsp_tables.obj $(SUBDIR)$(HPS)jitter.obj $(SUBDIR)$(HPS)kiss_fft.obj $(SUBDIR)$(HPS)kiss_fftr.obj $(SUBDIR)$(HPS)lpc.obj $(SUBDIR)$(HPS)lsp.obj $(SUBDIR)$(HPS)lsp_tables_nb.obj $(SUBDIR)$(HPS)ltp.obj $(SUBDIR)$(HPS)mdf.obj $(SUBDIR)$(HPS)modes.obj $(SUBDIR)$(HPS)modes_wb.obj $(SUBDIR)$(HPS)nb_celp.obj $(SUBDIR)$(HPS)preprocess.obj $(SUBDIR)$(HPS)quant_lsp.obj $(SUBDIR)$(HPS)resample.obj $(SUBDIR)$(HPS)sb_celp.obj $(SUBDIR)$(HPS)scal.obj $(SUBDIR)$(HPS)smallft.obj $(SUBDIR)$(HPS)speex.obj $(SUBDIR)$(HPS)speex_callbacks.obj $(SUBDIR)$(HPS)speex_header.obj $(SUBDIR)$(HPS)stereo.obj $(SUBDIR)$(HPS)testdenoise.obj $(SUBDIR)$(HPS)testecho.obj $(SUBDIR)$(HPS)testenc.obj $(SUBDIR)$(HPS)testenc_uwb.obj $(SUBDIR)$(HPS)testenc_wb.obj $(SUBDIR)$(HPS)testjitter.obj $(SUBDIR)$(HPS)vbr.obj $(SUBDIR)$(HPS)vq.obj $(SUBDIR)$(HPS)window.obj

!ifdef EXT_SPEEX_SPEEXDEC_EXE
$(EXT_SPEEX_SPEEXDEC_EXE): $(EXT_LIBOGG_LIB) $(EXT_SPEEX_LIB) $(SUBDIR)$(HPS)speexdec.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)wav_io.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)speexdec.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj file $(SUBDIR)$(HPS)wav_io.obj $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) library $(EXT_SPEEX_LIB) name $(EXT_SPEEX_SPEEXDEC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_SPEEX_SPEEXENC_EXE
$(EXT_SPEEX_SPEEXENC_EXE): $(EXT_LIBOGG_LIB) $(EXT_SPEEX_LIB) $(SUBDIR)$(HPS)speexenc.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)wav_io.obj $(SUBDIR)$(HPS)wave_out.obj $(SUBDIR)$(HPS)skeleton.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)speexenc.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj file $(SUBDIR)$(HPS)wav_io.obj file $(SUBDIR)$(HPS)wave_out.obj file $(SUBDIR)$(HPS)skeleton.obj $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) library $(EXT_SPEEX_LIB) name $(EXT_SPEEX_SPEEXENC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_SPEEX_LIB
$(EXT_SPEEX_LIB): $(OBJS)
	wlib -q -b -c $(EXT_SPEEX_LIB) -+$(SUBDIR)$(HPS)bits.obj -+$(SUBDIR)$(HPS)buffer.obj -+$(SUBDIR)$(HPS)cb_search.obj -+$(SUBDIR)$(HPS)exc_10_16_table.obj -+$(SUBDIR)$(HPS)exc_10_32_table.obj -+$(SUBDIR)$(HPS)exc_20_32_table.obj -+$(SUBDIR)$(HPS)exc_5_256_table.obj -+$(SUBDIR)$(HPS)exc_5_64_table.obj -+$(SUBDIR)$(HPS)exc_8_128_table.obj -+$(SUBDIR)$(HPS)fftwrap.obj -+$(SUBDIR)$(HPS)filterbank.obj -+$(SUBDIR)$(HPS)filters.obj -+$(SUBDIR)$(HPS)gain_table.obj -+$(SUBDIR)$(HPS)gain_table_lbr.obj -+$(SUBDIR)$(HPS)hexc_10_32_table.obj -+$(SUBDIR)$(HPS)hexc_table.obj -+$(SUBDIR)$(HPS)high_lsp_tables.obj -+$(SUBDIR)$(HPS)jitter.obj -+$(SUBDIR)$(HPS)kiss_fft.obj -+$(SUBDIR)$(HPS)kiss_fftr.obj -+$(SUBDIR)$(HPS)lpc.obj -+$(SUBDIR)$(HPS)lsp.obj -+$(SUBDIR)$(HPS)lsp_tables_nb.obj -+$(SUBDIR)$(HPS)ltp.obj -+$(SUBDIR)$(HPS)mdf.obj -+$(SUBDIR)$(HPS)modes.obj -+$(SUBDIR)$(HPS)modes_wb.obj -+$(SUBDIR)$(HPS)nb_celp.obj -+$(SUBDIR)$(HPS)preprocess.obj -+$(SUBDIR)$(HPS)quant_lsp.obj -+$(SUBDIR)$(HPS)resample.obj -+$(SUBDIR)$(HPS)sb_celp.obj -+$(SUBDIR)$(HPS)scal.obj -+$(SUBDIR)$(HPS)smallft.obj -+$(SUBDIR)$(HPS)speex.obj -+$(SUBDIR)$(HPS)speex_callbacks.obj -+$(SUBDIR)$(HPS)speex_header.obj -+$(SUBDIR)$(HPS)stereo.obj -+$(SUBDIR)$(HPS)testdenoise.obj -+$(SUBDIR)$(HPS)testecho.obj -+$(SUBDIR)$(HPS)testenc.obj -+$(SUBDIR)$(HPS)testenc_uwb.obj -+$(SUBDIR)$(HPS)testenc_wb.obj -+$(SUBDIR)$(HPS)testjitter.obj -+$(SUBDIR)$(HPS)vbr.obj -+$(SUBDIR)$(HPS)vq.obj -+$(SUBDIR)$(HPS)window.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_SPEEX_LIB) .symbolic

exe: $(EXT_SPEEX_SPEEXDEC_EXE) $(EXT_SPEEX_SPEEXENC_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

