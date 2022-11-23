# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
!ifeq TARGET_MSDOS 16
NO_LIB = 1
NO_EXE = 1
!endif
!ifndef NO_EXE
OGGINFO_EXE = $(SUBDIR)$(HPS)ogginfo.exe
OGGDEC_EXE = $(SUBDIR)$(HPS)oggdec.exe
OGGENC_EXE = $(SUBDIR)$(HPS)oggenc.exe
!endif

NOW_BUILDING = EXT_VORBTOOL_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)audio.obj $(SUBDIR)$(HPS)easyflac.obj $(SUBDIR)$(HPS)encode.obj $(SUBDIR)$(HPS)flac.obj $(SUBDIR)$(HPS)lyrics.obj $(SUBDIR)$(HPS)oggdec.obj $(SUBDIR)$(HPS)oggenc.obj $(SUBDIR)$(HPS)ogginfo2.obj $(SUBDIR)$(HPS)platform.obj $(SUBDIR)$(HPS)resample.obj $(SUBDIR)$(HPS)skeleton.obj $(SUBDIR)$(HPS)theora.obj $(SUBDIR)$(HPS)charset.obj $(SUBDIR)$(HPS)charset_test.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)iconvert.obj $(SUBDIR)$(HPS)makemap.obj $(SUBDIR)$(HPS)utf8.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: .symbolic

exe: $(OGGINFO_EXE) $(OGGDEC_EXE) $(OGGENC_EXE) .symbolic

!ifdef OGGINFO_EXE
$(OGGINFO_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(SUBDIR)$(HPS)ogginfo2.obj $(SUBDIR)$(HPS)theora.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)utf8.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)ogginfo2.obj file $(SUBDIR)$(HPS)theora.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj file $(SUBDIR)$(HPS)utf8.obj $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGINFO_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef OGGDEC_EXE
$(OGGDEC_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(SUBDIR)$(HPS)oggdec.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)oggdec.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGDEC_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef OGGENC_EXE
$(OGGENC_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(SUBDIR)$(HPS)oggenc.obj $(SUBDIR)$(HPS)getopt.obj $(SUBDIR)$(HPS)getopt1.obj $(SUBDIR)$(HPS)audio.obj $(SUBDIR)$(HPS)easyflac.obj $(SUBDIR)$(HPS)flac.obj $(SUBDIR)$(HPS)lyrics.obj $(SUBDIR)$(HPS)platform.obj $(SUBDIR)$(HPS)resample.obj $(SUBDIR)$(HPS)skeleton.obj $(SUBDIR)$(HPS)encode.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)oggenc.obj file $(SUBDIR)$(HPS)getopt.obj file $(SUBDIR)$(HPS)getopt1.obj file $(SUBDIR)$(HPS)audio.obj file $(SUBDIR)$(HPS)easyflac.obj file $(SUBDIR)$(HPS)flac.obj file $(SUBDIR)$(HPS)lyrics.obj file $(SUBDIR)$(HPS)platform.obj file $(SUBDIR)$(HPS)resample.obj file $(SUBDIR)$(HPS)skeleton.obj file $(SUBDIR)$(HPS)encode.obj $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGENC_EXE)
	@wlink @tmp.cmd
	upx --best $(OGGENC_EXE) # Whew! OGGENC.EXE comes out as a 1.7MB EXE file!
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

