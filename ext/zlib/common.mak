# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -fo=$(SUBDIR)$(HPS).obj -i.. -i"../.." -dHAVE_CONFIG_H -dSTDC

OBJS = $(SUBDIR)$(HPS)adler32.obj $(SUBDIR)$(HPS)compress.obj $(SUBDIR)$(HPS)crc32.obj $(SUBDIR)$(HPS)deflate.obj $(SUBDIR)$(HPS)gzclose.obj $(SUBDIR)$(HPS)gzlib.obj $(SUBDIR)$(HPS)gzread.obj $(SUBDIR)$(HPS)gzwrite.obj $(SUBDIR)$(HPS)infback.obj $(SUBDIR)$(HPS)inffast.obj $(SUBDIR)$(HPS)inflate.obj $(SUBDIR)$(HPS)inftrees.obj $(SUBDIR)$(HPS)trees.obj $(SUBDIR)$(HPS)uncompr.obj $(SUBDIR)$(HPS)zutil.obj

!ifdef EXT_ZLIB_MINIGZIP_EXE
$(EXT_ZLIB_MINIGZIP_EXE): $(EXT_ZLIB_LIB) $(SUBDIR)$(HPS)minigzip.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)minigzip.obj library $(EXT_ZLIB_LIB) name $(EXT_ZLIB_MINIGZIP_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_ZLIB_EXAMPLE_EXE
$(EXT_ZLIB_EXAMPLE_EXE): $(EXT_ZLIB_LIB) $(SUBDIR)$(HPS)example.obj
	%write tmp.cmd option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)example.obj library $(EXT_ZLIB_LIB) name $(EXT_ZLIB_EXAMPLE_EXE)
	@wlink @tmp.cmd
	@$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifndef EXT_ZLIB_LIB_NO_LIB
$(EXT_ZLIB_LIB): $(OBJS)
	wlib -q -b -c $(EXT_ZLIB_LIB) -+$(SUBDIR)$(HPS)adler32.obj -+$(SUBDIR)$(HPS)compress.obj -+$(SUBDIR)$(HPS)crc32.obj -+$(SUBDIR)$(HPS)deflate.obj -+$(SUBDIR)$(HPS)gzclose.obj -+$(SUBDIR)$(HPS)gzlib.obj -+$(SUBDIR)$(HPS)gzread.obj -+$(SUBDIR)$(HPS)gzwrite.obj -+$(SUBDIR)$(HPS)infback.obj -+$(SUBDIR)$(HPS)inffast.obj -+$(SUBDIR)$(HPS)inflate.obj -+$(SUBDIR)$(HPS)inftrees.obj -+$(SUBDIR)$(HPS)trees.obj -+$(SUBDIR)$(HPS)uncompr.obj -+$(SUBDIR)$(HPS)zutil.obj
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.c.obj:
	%write tmp.cmd $(CFLAGS_THIS) $(CFLAGS) $[@
	@$(CC) @tmp.cmd

all: lib exe .symbolic
       
lib: $(EXT_ZLIB_LIB) .symbolic

exe: $(EXT_ZLIB_MINIGZIP_EXE) $(EXT_ZLIB_EXAMPLE_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

