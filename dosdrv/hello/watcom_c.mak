
HERE = $+$(%cwd)$-

OBJS =     drvc.obj

CC       = wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -zl -q -zu -zdp -zff -zgf -zc -fpi87 -i../.. -dTARGET_MSDOS=16 -dMSDOS=1 -dNEAR_DRVVAR

all: dos86s dos86s/drv.exe

dos86s:
	mkdir -p dos86s

drva.asm:
	../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack

dos86s/drva.obj: drva.asm
	nasm -o $@ -f obj $(NASMFLAGS) $[@

.c.obj:
	$(CC) $(CFLAGS) $[@

../../hw/dos/dos86l/dos.lib:
	@cd ../../hw/dos
	@./make.sh build lib
	@cd $(HERE)

dos86s/drvc.obj: drvc.c
	$(CC) $(CFLAGS) $[@

dos86s/drvci.obj: drvci.c
	$(CC) $(CFLAGS) -nt=_INITTEXT -nd=_INIT -nc=INITCODE -g=_INIT_GROUP $[@

dos86s/drv.exe: dos86s/drvc.obj dos86s/drva.obj dos86s/drvci.obj
	%write tmp.cmd option quiet system dos
	%write tmp.cmd file dos86s/drva.obj
	%write tmp.cmd file dos86s/drvc.obj
	%write tmp.cmd file dos86s/drvci.obj
	%write tmp.cmd name dos86s/drv.exe
	%write tmp.cmd option map=dos86s/drv.map
	# special segment ordering
	%write tmp.cmd order clname CODE clname DATA segment CONST segment CONST2 segment _DATA clname BSS clname END clname INITBEGIN clname INITCODE clname INITEND
	# end segment ordering
	@wlink @tmp.cmd

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
