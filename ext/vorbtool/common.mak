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
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)oggdec.obj &
    $(PRFX)charset.obj &
    $(PRFX)charset_test.obj &
    $(PRFX)iconvert.obj &
    $(PRFX)makemap.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

all: lib exe .symbolic
       
lib: .symbolic

exe: $(OGGINFO_EXE) $(OGGDEC_EXE) $(OGGENC_EXE) .symbolic

!ifdef OGGINFO_EXE
OGGINFO_EXE_OBJS = &
    $(SUBDIR)$(HPS)ogginfo2.obj &
    $(SUBDIR)$(HPS)theora.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj &
    $(SUBDIR)$(HPS)utf8.obj

$(OGGINFO_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(OGGINFO_EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(OGGINFO_EXE_OBJS)} $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGINFO_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef OGGDEC_EXE
OGGDEC_EXE_OBJS = &
    $(SUBDIR)$(HPS)oggdec.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj

$(OGGDEC_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(OGGDEC_EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(OGGDEC_EXE_OBJS)} $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGDEC_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef OGGENC_EXE
OGGENC_EXE_OBJS = &
    $(SUBDIR)$(HPS)oggenc.obj &
    $(SUBDIR)$(HPS)getopt.obj &
    $(SUBDIR)$(HPS)getopt1.obj &
    $(SUBDIR)$(HPS)audio.obj &
    $(SUBDIR)$(HPS)easyflac.obj &
    $(SUBDIR)$(HPS)flac.obj &
    $(SUBDIR)$(HPS)lyrics.obj &
    $(SUBDIR)$(HPS)platform.obj &
    $(SUBDIR)$(HPS)resample.obj &
    $(SUBDIR)$(HPS)skeleton.obj &
    $(SUBDIR)$(HPS)encode.obj

$(OGGENC_EXE): $(EXT_LIBOGG_LIB) $(EXT_VORBIS_LIB) $(EXT_FLAC_LIB) $(OGGENC_EXE_OBJS)
        *wlink option quiet system $(WLINK_SYSTEM) file {$(OGGENC_EXE_OBJS)} $(EXT_LIBOGG_LIB_WLINK_LIBRARIES) $(EXT_VORBIS_LIB_WLINK_LIBRARIES) $(EXT_FLAC_LIB_WLINK_LIBRARIES) name $(OGGENC_EXE)
        upx --best $(OGGENC_EXE) # Whew! OGGENC.EXE comes out as a 1.7MB EXE file!
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

