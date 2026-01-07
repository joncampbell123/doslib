# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = HW_DOSBOXID_LIB
SUBDIR_CON = $(SUBDIR)_con
SUBDIR_DRV_ND = $(SUBDIR)_drv_nd

CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.."
CFLAGS_THIS_VXD = -s -zl -zc -nt=_TEXT
CFLAGS_THIS_DRV = -s -zl -zc -nt=_TEXT
CFLAGS_THIS_DRVN = -s -zl -zc -nt=_TEXT
CFLAGS_THIS_DRV_ND = -s -zl -zc -nt=_NDTEXT -nc=NDCODE

C_SOURCE =    iglib.c
MCR_EXE =     $(SUBDIR)$(HPS)mcr.$(EXEEXT)
UMC_EXE =     $(SUBDIR)$(HPS)umc.$(EXEEXT)
UMCN_EXE =    $(SUBDIR)$(HPS)umcn.$(EXEEXT)
TEST_EXE =    $(SUBDIR)$(HPS)test.$(EXEEXT)
SSHOT_EXE =   $(SUBDIR)$(HPS)sshot.$(EXEEXT)
VCAP_EXE =    $(SUBDIR)$(HPS)vcap.$(EXEEXT)
WCAP_EXE =    $(SUBDIR)$(HPS)wcap.$(EXEEXT)
KBSTAT_EXE =  $(SUBDIR)$(HPS)kbstat.$(EXEEXT)
EMTIME_EXE =  $(SUBDIR)$(HPS)emtime.$(EXEEXT)
KBINJECT_EXE =$(SUBDIR)$(HPS)kbinject.$(EXEEXT)
MSINJECT_EXE =$(SUBDIR)$(HPS)msinject.$(EXEEXT)
WATCHDOG_EXE =$(SUBDIR)$(HPS)watchdog.$(EXEEXT)
CYCLES_EXE =  $(SUBDIR)$(HPS)cycles.$(EXEEXT)
!ifndef TARGET_WINDOWS
ISADMA_EXE =  $(SUBDIR)$(HPS)isadma.$(EXEEXT)
VGAMI_EXE =   $(SUBDIR)$(HPS)vgami.$(EXEEXT)
IRQ_EXE =     $(SUBDIR)$(HPS)irq.$(EXEEXT)
NMI_EXE =     $(SUBDIR)$(HPS)nmi.$(EXEEXT)
IRQINFO_EXE = $(SUBDIR)$(HPS)irqinfo.$(EXEEXT)
ENVBEIG_EXE = $(SUBDIR)$(HPS)envbeig.$(EXEEXT)
!endif

OBJS = &
    $(SUBDIR)$(HPS)iglib.obj &
    $(SUBDIR)$(HPS)igregio.obj &
    $(SUBDIR)$(HPS)igrselio.obj &
    $(SUBDIR)$(HPS)igprobe.obj &
    $(SUBDIR)$(HPS)igreset.obj &
    $(SUBDIR)$(HPS)igrident.obj &
    $(SUBDIR)$(HPS)igverstr.obj &
    $(SUBDIR)$(HPS)igdbgmsg.obj
DRV_OBJS = &
    $(SUBDIR_DRV)$(HPS)iglib.obj &
    $(SUBDIR_DRV_ND)$(HPS)igregio.obj &
    $(SUBDIR_DRV_ND)$(HPS)igrselio.obj &
    $(SUBDIR_DRV)$(HPS)igprobe.obj &
    $(SUBDIR_DRV)$(HPS)igreset.obj &
    $(SUBDIR_DRV)$(HPS)igrident.obj &
    $(SUBDIR_DRV)$(HPS)igverstr.obj &
    $(SUBDIR_DRV)$(HPS)igdbgmsg.obj
DRVN_OBJS = &
    $(SUBDIR_DRVN)$(HPS)iglib.obj &
    $(SUBDIR_DRVN)$(HPS)igregio.obj &
    $(SUBDIR_DRVN)$(HPS)igrselio.obj &
    $(SUBDIR_DRVN)$(HPS)igprobe.obj &
    $(SUBDIR_DRVN)$(HPS)igreset.obj &
    $(SUBDIR_DRVN)$(HPS)igrident.obj &
    $(SUBDIR_DRVN)$(HPS)igverstr.obj &
    $(SUBDIR_DRVN)$(HPS)igdbgmsg.obj

$(SUBDIR) $(SUBDIR_CON) $(SUBDIR_DRV) $(SUBDIR_DRV_ND) $(SUBDIR_DRVN):
    mkdir -p $@

$(HW_DOSBOXID_LIB): $(OBJS)
    *wlib -q -b -c $(HW_DOSBOXID_LIB) &
    -+$(SUBDIR)$(HPS)iglib.obj &
    -+$(SUBDIR)$(HPS)igregio.obj &
    -+$(SUBDIR)$(HPS)igrselio.obj &
    -+$(SUBDIR)$(HPS)igprobe.obj &
    -+$(SUBDIR)$(HPS)igreset.obj &
    -+$(SUBDIR)$(HPS)igrident.obj &
    -+$(SUBDIR)$(HPS)igverstr.obj &
    -+$(SUBDIR)$(HPS)igdbgmsg.obj

!ifdef HW_DOSBOXID_LIB_DRV
$(HW_DOSBOXID_LIB_DRV): $(SUBDIR_DRV) $(SUBDIR_DRV_ND) $(DRV_OBJS)
    *wlib -q -b -c $(HW_DOSBOXID_LIB_DRV) &
    -+$(SUBDIR_DRV)$(HPS)iglib.obj &
    -+$(SUBDIR_DRV_ND)$(HPS)igregio.obj &
    -+$(SUBDIR_DRV_ND)$(HPS)igrselio.obj &
    -+$(SUBDIR_DRV)$(HPS)igprobe.obj &
    -+$(SUBDIR_DRV)$(HPS)igreset.obj &
    -+$(SUBDIR_DRV)$(HPS)igrident.obj &
    -+$(SUBDIR_DRV)$(HPS)igverstr.obj &
    -+$(SUBDIR_DRV)$(HPS)igdbgmsg.obj
!endif

!ifdef HW_DOSBOXID_LIB_DRVN
$(HW_DOSBOXID_LIB_DRVN): $(SUBDIR_DRVN) $(DRVN_OBJS)
    *wlib -q -b -c $(HW_DOSBOXID_LIB_DRVN) &
    -+$(SUBDIR_DRVN)$(HPS)iglib.obj &
    -+$(SUBDIR_DRVN)$(HPS)igregio.obj &
    -+$(SUBDIR_DRVN)$(HPS)igrselio.obj &
    -+$(SUBDIR_DRVN)$(HPS)igprobe.obj &
    -+$(SUBDIR_DRVN)$(HPS)igreset.obj &
    -+$(SUBDIR_DRVN)$(HPS)igrident.obj &
    -+$(SUBDIR_DRVN)$(HPS)igverstr.obj &
    -+$(SUBDIR_DRVN)$(HPS)igdbgmsg.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c{$(SUBDIR)}.obj:
!ifdef TARGET_VXD
	@$(CC) $(CFLAGS_THIS) $(CFLAGS) $(CFLAGS_THIS_VXD) $[@
!else
	@$(CC) $(CFLAGS_THIS) $(CFLAGS) $[@
!endif

.c{$(SUBDIR_CON)}.obj:
	@$(CC) $(CFLAGS_THIS) $(CFLAGS_CON) $[@

!ifdef HW_DOSBOXID_LIB_DRV
.c{$(SUBDIR_DRV)}.obj:
	@$(CC) $(CFLAGS_THIS) $(CFLAGS) $(CFLAGS_THIS_DRV) $[@
.c{$(SUBDIR_DRV_ND)}.obj:
	@$(CC) $(CFLAGS_THIS) $(CFLAGS) $(CFLAGS_THIS_DRV_ND) $[@
!endif

!ifdef HW_DOSBOXID_LIB_DRVN
.c{$(SUBDIR_DRVN)}.obj:
	@$(CC) $(CFLAGS_THIS) $(CFLAGS) $(CFLAGS_THIS_DRVN) $[@
!endif

all: $(SUBDIR) $(OMFSEGDG) lib exe

lib: $(HW_DOSBOXID_LIB) $(HW_DOSBOXID_LIB_DRV) $(HW_DOSBOXID_LIB_DRVN) .symbolic

exe: $(TEST_EXE) $(MCR_EXE) $(UMC_EXE) $(UMCN_EXE) $(SSHOT_EXE) $(VCAP_EXE) $(WCAP_EXE) $(KBSTAT_EXE) $(KBINJECT_EXE) $(MSINJECT_EXE) $(ISADMA_EXE) $(IRQ_EXE) $(NMI_EXE) $(IRQINFO_EXE) $(EMTIME_EXE) $(WATCHDOG_EXE) $(CYCLES_EXE) $(VGAMI_EXE) $(ENVBEIG_EXE) .symbolic

LINK_EXE_DEPENDENCIES = $(HW_DOSBOXID_LIB) $(HW_DOSBOXID_LIB_DEPENDENCIES) $(HW_8254_LIB) $(HW_8254_LIB_DEPENDENCIES) $(HW_DOS_LIB) $(HW_DOS_LIB_DEPENDENCIES) $(HW_8237_LIB) $(HW_8237_LIB_DEPENDENCIES)
LINK_EXE_LIBS = $(HW_DOSBOXID_LIB_WLINK_LIBRARIES) $(HW_8254_LIB_WLINK_LIBRARIES) $(HW_8237_LIB_WLINK_LIBRARIES) $(HW_DOS_LIB_WLINK_LIBRARIES)

!ifdef UMC_EXE
$(UMC_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)umc.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)umc.obj $(LINK_EXE_LIBS) name $(UMC_EXE) option map=$(UMC_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef UMCN_EXE
$(UMCN_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)umcn.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)umcn.obj $(LINK_EXE_LIBS) name $(UMCN_EXE) option map=$(UMCN_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef MCR_EXE
$(MCR_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)mcr.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)mcr.obj $(LINK_EXE_LIBS) name $(MCR_EXE) option map=$(MCR_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef TEST_EXE
$(TEST_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)test.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)test.obj $(LINK_EXE_LIBS) name $(TEST_EXE) option map=$(TEST_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef SSHOT_EXE
$(SSHOT_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)sshot.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)sshot.obj $(LINK_EXE_LIBS) name $(SSHOT_EXE) option map=$(SSHOT_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VCAP_EXE
$(VCAP_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)vcap.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)vcap.obj $(LINK_EXE_LIBS) name $(VCAP_EXE) option map=$(VCAP_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef WCAP_EXE
$(WCAP_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)wcap.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)wcap.obj $(LINK_EXE_LIBS) name $(WCAP_EXE) option map=$(WCAP_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef KBSTAT_EXE
$(KBSTAT_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)kbstat.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)kbstat.obj $(LINK_EXE_LIBS) name $(KBSTAT_EXE) option map=$(KBSTAT_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef KBINJECT_EXE
$(KBINJECT_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)kbinject.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)kbinject.obj $(LINK_EXE_LIBS) name $(KBINJECT_EXE) option map=$(KBINJECT_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef MSINJECT_EXE
$(MSINJECT_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)msinject.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)msinject.obj $(LINK_EXE_LIBS) name $(MSINJECT_EXE) option map=$(MSINJECT_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef ISADMA_EXE
$(ISADMA_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)isadma.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)isadma.obj $(LINK_EXE_LIBS) name $(ISADMA_EXE) option map=$(ISADMA_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef VGAMI_EXE
$(VGAMI_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)vgami.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)vgami.obj $(LINK_EXE_LIBS) name $(VGAMI_EXE) option map=$(VGAMI_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef IRQ_EXE
$(IRQ_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)irq.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)irq.obj $(LINK_EXE_LIBS) name $(IRQ_EXE) option map=$(IRQ_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef NMI_EXE
$(NMI_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)nmi.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)nmi.obj $(LINK_EXE_LIBS) name $(NMI_EXE) option map=$(NMI_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef IRQINFO_EXE
$(IRQINFO_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)irqinfo.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)irqinfo.obj $(LINK_EXE_LIBS) name $(IRQINFO_EXE) option map=$(IRQINFO_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef ENVBEIG_EXE
$(ENVBEIG_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)envbeig.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)envbeig.obj $(LINK_EXE_LIBS) name $(ENVBEIG_EXE) option map=$(ENVBEIG_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EMTIME_EXE
$(EMTIME_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)emtime.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)emtime.obj $(LINK_EXE_LIBS) name $(EMTIME_EXE) option map=$(EMTIME_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef WATCHDOG_EXE
$(WATCHDOG_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)watchdog.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)watchdog.obj $(LINK_EXE_LIBS) name $(WATCHDOG_EXE) option map=$(WATCHDOG_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef CYCLES_EXE
$(CYCLES_EXE): $(SUBDIR_CON) $(SUBDIR_CON)$(HPS)cycles.obj $(LINK_EXE_DEPENDENCIES)
	@*wlink option quiet system $(WLINK_CON_SYSTEM) file $(SUBDIR_CON)$(HPS)cycles.obj $(LINK_EXE_LIBS) name $(CYCLES_EXE) option map=$(CYCLES_EXE).map
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(SUBDIR_CON)$(HPS)*.obj
          del $(SUBDIR_DRV_ND)$(HPS)*.obj
          del $(HW_DOSBOXID_LIB)
          del tmp.cmd
          @echo Cleaning done

