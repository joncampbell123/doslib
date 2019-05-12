# this makefile is included from all the dos*.mak files, do not use directly
# NTS: HPS is either \ (DOS) or / (Linux)
NOW_BUILDING = EXT_ZLIB_LIB
CFLAGS_THIS = -fr=nul -fo=$@ -i.. -i"../.." -dHAVE_CONFIG_H -dSTDC

OBJS = &
    $(PRFX)adler32.obj &
    $(PRFX)compress.obj &
    $(PRFX)crc32.obj &
    $(PRFX)deflate.obj &
    $(PRFX)gzclose.obj &
    $(PRFX)gzlib.obj &
    $(PRFX)gzread.obj &
    $(PRFX)gzwrite.obj &
    $(PRFX)infback.obj &
    $(PRFX)inffast.obj &
    $(PRFX)inflate.obj &
    $(PRFX)inftrees.obj &
    $(PRFX)trees.obj &
    $(PRFX)uncompr.obj &
    $(PRFX)zutil.obj

all: lib exe .symbolic
       
!ifdef EXT_ZLIB_MINIGZIP_EXE
$(EXT_ZLIB_MINIGZIP_EXE): $(EXT_ZLIB_LIB) $(SUBDIR)$(HPS)minigzip.obj
        *wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)minigzip.obj library $(EXT_ZLIB_LIB) name $(EXT_ZLIB_MINIGZIP_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifdef EXT_ZLIB_EXAMPLE_EXE
$(EXT_ZLIB_EXAMPLE_EXE): $(EXT_ZLIB_LIB) $(SUBDIR)$(HPS)example.obj
        *wlink option quiet system $(WLINK_SYSTEM) file $(SUBDIR)$(HPS)example.obj library $(EXT_ZLIB_LIB) name $(EXT_ZLIB_EXAMPLE_EXE)
        @$(COPY) ..$(HPS)..$(HPS)dos32a.dat $(SUBDIR)$(HPS)dos4gw.exe
!endif

!ifndef EXT_ZLIB_LIB_NO_LIB
PRFX = $(SUBDIR)$(HPS)
LIB_OBJS = $+$(OBJS)$-
PRFX = -+$(SUBDIR)$(HPS)
LIB_CMDS = $+$(OBJS)$-

$(EXT_ZLIB_LIB): $(LIB_OBJS)
        *wlib -q -b -c $(EXT_ZLIB_LIB) $(LIB_CMDS)
!endif

# NTS we have to construct the command line into tmp.cmd because for MS-DOS
# systems all arguments would exceed the pitiful 128 char command line limit
.C.OBJ:
        $(CC) $(CFLAGS_THIS) $(CFLAGS) $[@

lib: $(EXT_ZLIB_LIB) .symbolic

exe: $(EXT_ZLIB_MINIGZIP_EXE) $(EXT_ZLIB_EXAMPLE_EXE) .symbolic

clean: .SYMBOLIC
          del $(SUBDIR)$(HPS)*.obj
          del tmp.cmd
          @echo Cleaning done

