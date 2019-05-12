# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_JPEG_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = &
    $(PRFX)jaricom.obj &
    $(PRFX)jcapimin.obj &
    $(PRFX)jcapistd.obj &
    $(PRFX)jcarith.obj &
    $(PRFX)jccoefct.obj &
    $(PRFX)jccolor.obj &
    $(PRFX)jcdctmgr.obj &
    $(PRFX)jchuff.obj &
    $(PRFX)jcinit.obj &
    $(PRFX)jcmainct.obj &
    $(PRFX)jcmarker.obj &
    $(PRFX)jcmaster.obj &
    $(PRFX)jcomapi.obj &
    $(PRFX)jcparam.obj &
    $(PRFX)jcprepct.obj &
    $(PRFX)jcsample.obj &
    $(PRFX)jctrans.obj &
    $(PRFX)jdapimin.obj &
    $(PRFX)jdapistd.obj &
    $(PRFX)jdarith.obj &
    $(PRFX)jdatadst.obj &
    $(PRFX)jdatasrc.obj &
    $(PRFX)jdcoefct.obj &
    $(PRFX)jdcolor.obj &
    $(PRFX)jddctmgr.obj &
    $(PRFX)jdhuff.obj &
    $(PRFX)jdinput.obj &
    $(PRFX)jdmainct.obj &
    $(PRFX)jdmarker.obj &
    $(PRFX)jdmaster.obj &
    $(PRFX)jdmerge.obj &
    $(PRFX)jdpostct.obj &
    $(PRFX)jdsample.obj &
    $(PRFX)jdtrans.obj &
    $(PRFX)jerror.obj &
    $(PRFX)jfdctflt.obj &
    $(PRFX)jfdctfst.obj &
    $(PRFX)jfdctint.obj &
    $(PRFX)jidctflt.obj &
    $(PRFX)jidctfst.obj &
    $(PRFX)jidctint.obj &
    $(PRFX)jmemansi.obj &
    $(PRFX)jquant1.obj &
    $(PRFX)jquant2.obj &
    $(PRFX)jutils.obj &
    $(PRFX)rdbmp.obj &
    $(PRFX)rdcolmap.obj &
    $(PRFX)rdgif.obj &
    $(PRFX)rdjpgcom.obj &
    $(PRFX)rdppm.obj &
    $(PRFX)rdrle.obj &
    $(PRFX)rdswitch.obj &
    $(PRFX)rdtarga.obj &
    $(PRFX)wrbmp.obj &
    $(PRFX)wrgif.obj &
    $(PRFX)wrjpgcom.obj &
    $(PRFX)wrppm.obj &
    $(PRFX)wrrle.obj &
    $(PRFX)wrtarga.obj &
    $(PRFX)jmemmgr.obj &
    $(PRFX)cdjpeg.obj

all: lib exe .symbolic
       
!ifdef EXT_JPEG_CJPEG_EXE
$(EXT_JPEG_CJPEG_EXE): $(EXT_JPEG_LIB) $(SUBDIR)$(HPS)cjpeg.obj
        *wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)cjpeg.obj library $(EXT_JPEG_LIB) name $(EXT_JPEG_CJPEG_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif
!ifdef EXT_JPEG_DJPEG_EXE
$(EXT_JPEG_DJPEG_EXE): $(EXT_JPEG_LIB) $(SUBDIR)$(HPS)djpeg.obj
        *wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)djpeg.obj library $(EXT_JPEG_LIB) name $(EXT_JPEG_DJPEG_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif
!ifdef EXT_JPEG_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_JPEG_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_JPEG_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_JPEG_LIB) .symbolic

exe: $(EXT_JPEG_CJPEG_EXE) $(EXT_JPEG_DJPEG_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

