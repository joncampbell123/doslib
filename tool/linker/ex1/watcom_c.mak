
HERE = $+$(%cwd)$-

OBJS =     drvc.obj

CC       = wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -zl -q -zu -zdp -zff -zgf -zc -fpi87 -i../../.. -dTARGET_MSDOS=16 -dMSDOS=1 -dNEAR_DRVVAR

all: dos86s dos86s/drv.exe

dos86s:
	mkdir -p dos86s

entry.asm:
	../../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack

dos86s/entry.obj: entry.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@

.C.OBJ:
	$(CC) $(CFLAGS) $[@

../../hw/dos/dos86l/dos.lib:
	@cd ../../hw/dos
	@./make.sh build lib
	@cd $(HERE)

dos86s/drvc.obj: drvc.c
	$(CC) $(CFLAGS) $[@

dos86s/drv.exe: dos86s/drvc.obj dos86s/entry.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file dos86s/entry.obj
	%write tmp.cmd file dos86s/drvc.obj
	%write tmp.cmd name dos86s/drv.exe
	%write tmp.cmd option map=dos86s/drv.map
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
