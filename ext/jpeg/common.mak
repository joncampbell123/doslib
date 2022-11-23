# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_JPEG_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H

OBJS = $(SUBDIR)$(HPS)jaricom.obj $(SUBDIR)$(HPS)jcapimin.obj $(SUBDIR)$(HPS)jcapistd.obj $(SUBDIR)$(HPS)jcarith.obj $(SUBDIR)$(HPS)jccoefct.obj $(SUBDIR)$(HPS)jccolor.obj $(SUBDIR)$(HPS)jcdctmgr.obj $(SUBDIR)$(HPS)jchuff.obj $(SUBDIR)$(HPS)jcinit.obj $(SUBDIR)$(HPS)jcmainct.obj $(SUBDIR)$(HPS)jcmarker.obj $(SUBDIR)$(HPS)jcmaster.obj $(SUBDIR)$(HPS)jcomapi.obj $(SUBDIR)$(HPS)jcparam.obj $(SUBDIR)$(HPS)jcprepct.obj $(SUBDIR)$(HPS)jcsample.obj $(SUBDIR)$(HPS)jctrans.obj $(SUBDIR)$(HPS)jdapimin.obj $(SUBDIR)$(HPS)jdapistd.obj $(SUBDIR)$(HPS)jdarith.obj $(SUBDIR)$(HPS)jdatadst.obj $(SUBDIR)$(HPS)jdatasrc.obj $(SUBDIR)$(HPS)jdcoefct.obj $(SUBDIR)$(HPS)jdcolor.obj $(SUBDIR)$(HPS)jddctmgr.obj $(SUBDIR)$(HPS)jdhuff.obj $(SUBDIR)$(HPS)jdinput.obj $(SUBDIR)$(HPS)jdmainct.obj $(SUBDIR)$(HPS)jdmarker.obj $(SUBDIR)$(HPS)jdmaster.obj $(SUBDIR)$(HPS)jdmerge.obj $(SUBDIR)$(HPS)jdpostct.obj $(SUBDIR)$(HPS)jdsample.obj $(SUBDIR)$(HPS)jdtrans.obj $(SUBDIR)$(HPS)jerror.obj $(SUBDIR)$(HPS)jfdctflt.obj $(SUBDIR)$(HPS)jfdctfst.obj $(SUBDIR)$(HPS)jfdctint.obj $(SUBDIR)$(HPS)jidctflt.obj $(SUBDIR)$(HPS)jidctfst.obj $(SUBDIR)$(HPS)jidctint.obj $(SUBDIR)$(HPS)jmemansi.obj $(SUBDIR)$(HPS)jquant1.obj $(SUBDIR)$(HPS)jquant2.obj $(SUBDIR)$(HPS)jutils.obj $(SUBDIR)$(HPS)rdbmp.obj $(SUBDIR)$(HPS)rdcolmap.obj $(SUBDIR)$(HPS)rdgif.obj $(SUBDIR)$(HPS)rdjpgcom.obj $(SUBDIR)$(HPS)rdppm.obj $(SUBDIR)$(HPS)rdrle.obj $(SUBDIR)$(HPS)rdswitch.obj $(SUBDIR)$(HPS)rdtarga.obj $(SUBDIR)$(HPS)wrbmp.obj $(SUBDIR)$(HPS)wrgif.obj $(SUBDIR)$(HPS)wrjpgcom.obj $(SUBDIR)$(HPS)wrppm.obj $(SUBDIR)$(HPS)wrrle.obj $(SUBDIR)$(HPS)wrtarga.obj $(SUBDIR)$(HPS)jmemmgr.obj $(SUBDIR)$(HPS)cdjpeg.obj

!ifdef EXT_JPEG_CJPEG_EXE
$(EXT_JPEG_CJPEG_EXE): $(EXT_JPEG_LIB) $(SUBDIR)$(HPS)cjpeg.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)cjpeg.obj library $(EXT_JPEG_LIB) name $(EXT_JPEG_CJPEG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif
!ifdef EXT_JPEG_DJPEG_EXE
$(EXT_JPEG_DJPEG_EXE): $(EXT_JPEG_LIB) $(SUBDIR)$(HPS)djpeg.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)djpeg.obj library $(EXT_JPEG_LIB) name $(EXT_JPEG_DJPEG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif
!ifdef EXT_JPEG_LIB
$(EXT_JPEG_LIB): $(OBJS)
	wlib -q -b -c $(EXT_JPEG_LIB) -+$(SUBDIR)$(HPS)jaricom.obj -+$(SUBDIR)$(HPS)jcapimin.obj -+$(SUBDIR)$(HPS)jcapistd.obj -+$(SUBDIR)$(HPS)jcarith.obj -+$(SUBDIR)$(HPS)jccoefct.obj -+$(SUBDIR)$(HPS)jccolor.obj -+$(SUBDIR)$(HPS)jcdctmgr.obj -+$(SUBDIR)$(HPS)jchuff.obj -+$(SUBDIR)$(HPS)jcinit.obj -+$(SUBDIR)$(HPS)jcmainct.obj -+$(SUBDIR)$(HPS)jcmarker.obj -+$(SUBDIR)$(HPS)jcmaster.obj -+$(SUBDIR)$(HPS)jcomapi.obj -+$(SUBDIR)$(HPS)jcparam.obj -+$(SUBDIR)$(HPS)jcprepct.obj -+$(SUBDIR)$(HPS)jcsample.obj -+$(SUBDIR)$(HPS)jctrans.obj -+$(SUBDIR)$(HPS)jdapimin.obj -+$(SUBDIR)$(HPS)jdapistd.obj -+$(SUBDIR)$(HPS)jdarith.obj -+$(SUBDIR)$(HPS)jdatadst.obj -+$(SUBDIR)$(HPS)jdatasrc.obj -+$(SUBDIR)$(HPS)jdcoefct.obj -+$(SUBDIR)$(HPS)jdcolor.obj -+$(SUBDIR)$(HPS)jddctmgr.obj -+$(SUBDIR)$(HPS)jdhuff.obj -+$(SUBDIR)$(HPS)jdinput.obj -+$(SUBDIR)$(HPS)jdmainct.obj -+$(SUBDIR)$(HPS)jdmarker.obj -+$(SUBDIR)$(HPS)jdmaster.obj -+$(SUBDIR)$(HPS)jdmerge.obj -+$(SUBDIR)$(HPS)jdpostct.obj -+$(SUBDIR)$(HPS)jdsample.obj -+$(SUBDIR)$(HPS)jdtrans.obj -+$(SUBDIR)$(HPS)jerror.obj -+$(SUBDIR)$(HPS)jfdctflt.obj -+$(SUBDIR)$(HPS)jfdctfst.obj -+$(SUBDIR)$(HPS)jfdctint.obj -+$(SUBDIR)$(HPS)jidctflt.obj -+$(SUBDIR)$(HPS)jidctfst.obj -+$(SUBDIR)$(HPS)jidctint.obj -+$(SUBDIR)$(HPS)jmemansi.obj -+$(SUBDIR)$(HPS)jquant1.obj -+$(SUBDIR)$(HPS)jquant2.obj -+$(SUBDIR)$(HPS)jutils.obj -+$(SUBDIR)$(HPS)rdbmp.obj -+$(SUBDIR)$(HPS)rdcolmap.obj -+$(SUBDIR)$(HPS)rdgif.obj -+$(SUBDIR)$(HPS)rdjpgcom.obj -+$(SUBDIR)$(HPS)rdppm.obj -+$(SUBDIR)$(HPS)rdrle.obj -+$(SUBDIR)$(HPS)rdswitch.obj -+$(SUBDIR)$(HPS)rdtarga.obj -+$(SUBDIR)$(HPS)wrbmp.obj -+$(SUBDIR)$(HPS)wrgif.obj -+$(SUBDIR)$(HPS)wrjpgcom.obj -+$(SUBDIR)$(HPS)wrppm.obj -+$(SUBDIR)$(HPS)wrrle.obj -+$(SUBDIR)$(HPS)wrtarga.obj -+$(SUBDIR)$(HPS)jmemmgr.obj -+$(SUBDIR)$(HPS)cdjpeg.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_JPEG_LIB) .symbolic

exe: $(EXT_JPEG_CJPEG_EXE) $(EXT_JPEG_DJPEG_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

