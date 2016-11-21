
HERE = $+$(%cwd)$-

OBJS =     omfsegfl.obj

CC       = wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -q -zdp -zff -zgf -zc -fpi87 -i../.. -dTARGET_MSDOS=16 -dMSDOS=1

all: dos86s dos86s/omfsegfl.exe

dos86s:
	mkdir -p dos86s

.C.OBJ:
	$(CC) $(CFLAGS) $[@

dos86s/omfsegfl.obj: omfsegfl.c
	$(CC) $(CFLAGS) $[@

dos86s/omfsegfl.exe: dos86s/omfsegfl.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file dos86s/omfsegfl.obj
	%write tmp.cmd name dos86s/omfsegfl.exe
	%write tmp.cmd option map=dos86s/omfsegfl.map
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
