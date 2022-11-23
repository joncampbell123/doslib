
# TODO: OS/2 target: What can we #define to tell the header files which OS/2
#       environment we're doing? (Command prompt app. vs Presentation Manager app vs.
#       "fullscreen" app.)

# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)

CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.."
NOW_BUILDING = FMT_OMF_LIB

OBJS =        $(SUBDIR)$(HPS)oextdefs.obj $(SUBDIR)$(HPS)oextdeft.obj $(SUBDIR)$(HPS)ofixupps.obj $(SUBDIR)$(HPS)ofixuppt.obj $(SUBDIR)$(HPS)ogrpdefs.obj $(SUBDIR)$(HPS)olnames.obj $(SUBDIR)$(HPS)omfcstr.obj $(SUBDIR)$(HPS)omfctx.obj $(SUBDIR)$(HPS)omfrec.obj $(SUBDIR)$(HPS)omfrecs.obj $(SUBDIR)$(HPS)omledata.obj $(SUBDIR)$(HPS)opubdefs.obj $(SUBDIR)$(HPS)opubdeft.obj $(SUBDIR)$(HPS)osegdefs.obj $(SUBDIR)$(HPS)osegdeft.obj $(SUBDIR)$(HPS)opledata.obj $(SUBDIR)$(HPS)omfctxnm.obj $(SUBDIR)$(HPS)omfctxrf.obj $(SUBDIR)$(HPS)omfctxlf.obj $(SUBDIR)$(HPS)optheadr.obj $(SUBDIR)$(HPS)opextdef.obj $(SUBDIR)$(HPS)opfixupp.obj $(SUBDIR)$(HPS)opgrpdef.obj $(SUBDIR)$(HPS)oppubdef.obj $(SUBDIR)$(HPS)opsegdef.obj $(SUBDIR)$(HPS)oplnames.obj $(SUBDIR)$(HPS)odlnames.obj $(SUBDIR)$(HPS)odextdef.obj $(SUBDIR)$(HPS)odfixupp.obj $(SUBDIR)$(HPS)odgrpdef.obj $(SUBDIR)$(HPS)odledata.obj $(SUBDIR)$(HPS)odlidata.obj $(SUBDIR)$(HPS)odpubdef.obj $(SUBDIR)$(HPS)odsegdef.obj $(SUBDIR)$(HPS)odtheadr.obj $(SUBDIR)$(HPS)omfctxwf.obj $(SUBDIR)$(HPS)omfrecw.obj $(SUBDIR)$(HPS)owfixupp.obj

!ifeq TARGET_MSDOS 32
! ifeq TARGET_WINDOWS 31
!  define NO_EXE
! endif
!endif

!ifndef NO_EXE
OMFDUMP_EXE = $(SUBDIR)$(HPS)omfdump.$(EXEEXT)
OMFSEGDG_EXE = $(SUBDIR)$(HPS)omfsegdg.$(EXEEXT)
!endif

$(FMT_OMF_LIB): $(OBJS)
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)oextdefs.obj -+$(SUBDIR)$(HPS)oextdeft.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)ofixupps.obj -+$(SUBDIR)$(HPS)ofixuppt.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)ogrpdefs.obj -+$(SUBDIR)$(HPS)olnames.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omfcstr.obj  -+$(SUBDIR)$(HPS)omfctx.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omfrec.obj   -+$(SUBDIR)$(HPS)omfrecs.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omledata.obj -+$(SUBDIR)$(HPS)opubdefs.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)opubdeft.obj -+$(SUBDIR)$(HPS)osegdefs.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)osegdeft.obj -+$(SUBDIR)$(HPS)opledata.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omfctxnm.obj -+$(SUBDIR)$(HPS)omfctxrf.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omfctxlf.obj -+$(SUBDIR)$(HPS)optheadr.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)opextdef.obj -+$(SUBDIR)$(HPS)opfixupp.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)opgrpdef.obj -+$(SUBDIR)$(HPS)oppubdef.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)opsegdef.obj -+$(SUBDIR)$(HPS)oplnames.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)odlnames.obj -+$(SUBDIR)$(HPS)odextdef.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)odfixupp.obj -+$(SUBDIR)$(HPS)odgrpdef.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)odledata.obj -+$(SUBDIR)$(HPS)odlidata.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)odpubdef.obj -+$(SUBDIR)$(HPS)odsegdef.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)odtheadr.obj -+$(SUBDIR)$(HPS)omfctxwf.obj
	wlib -q -b -c $(FMT_OMF_LIB) -+$(SUBDIR)$(HPS)omfrecw.obj  -+$(SUBDIR)$(HPS)owfixupp.obj

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS_CON) $[@
	@$(CC) @tmp.cmd

all: lib exe

exe: $(OMFDUMP_EXE) $(OMFSEGDG_EXE) .symbolic

lib: $(FMT_OMF_LIB) .symbolic

!ifdef OMFDUMP_EXE
$(OMFDUMP_EXE): $(FMT_OMF_LIB) $(FMT_OMF_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)omfdump.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)omfdump.obj $(FMT_OMF_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(OMFDUMP_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(OMFDUMP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(OMFDUMP_EXE)
	@wbind $(OMFDUMP_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(OMFDUMP_EXE)
! endif
!endif

!ifdef OMFSEGDG_EXE
$(OMFSEGDG_EXE): $(FMT_OMF_LIB) $(FMT_OMF_LIB_DEPENDENCIES) $(SUBDIR)$(HPS)omfsegdg.obj
	%write tmp.cmd option quiet system $(WLINK_CON_SYSTEM) $(WLINK_FLAGS) file $(SUBDIR)$(HPS)omfsegdg.obj $(FMT_OMF_LIB_WLINK_LIBRARIES)
	%write tmp.cmd option map=$(OMFSEGDG_EXE).map
! ifdef TARGET_WINDOWS
!  ifeq TARGET_MSDOS 16
	%write tmp.cmd segment TYPE CODE PRELOAD FIXED DISCARDABLE SHARED
	%write tmp.cmd segment TYPE DATA PRELOAD MOVEABLE
!  endif
! endif
	%write tmp.cmd name $(OMFSEGDG_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
! ifdef WIN386
	@$(WIN386_EXE_TO_REX_IF_REX) $(OMFSEGDG_EXE)
	@wbind $(OMFSEGDG_EXE) -q -n
! endif
! ifdef WIN_NE_SETVER_BUILD
	$(WIN_NE_SETVER_BUILD) $(OMFSEGDG_EXE)
! endif
!endif

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del $(FMT_OMF_LIB)
          del tmp.cmd
          @echo Cleaning done

