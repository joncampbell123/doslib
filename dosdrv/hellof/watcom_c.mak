
HERE = $+$(%cwd)$-

OBJS =     drvc.obj

CC       = *wcc
CFLAGS   = -zq -ms -s -bt=dos -oilrsm -fr=nul -wx -0 -fo=dos86s/.obj -zl -q -zu -zdp -zff -zgf -zc -fpi87 -i"../.." -dTARGET_MSDOS=16 -dMSDOS=1 -dNEAR_DRVVAR -g=DGROUP -nt=_TEXT -nc=CODE

all: dos86s dos86s/drv.sys

dos86s:
        mkdir -p dos86s

drva.asm:
        ../../hw/dos/devhdgen.pl --asm $@ --name "hello$$" --type c --c-openclose --c-out-busy --no-stack --no-stub --ds-is-cs --int-stub
        echo "group DGROUP _END _BEGIN _TEXT _DATA" >>$@

dos86s/drva.obj: drva.asm
        nasm -o $@ -f obj $(NASMFLAGS) $[@

.C.OBJ:
        $(CC) $(CFLAGS) $[@

../../hw/dos/dos86l/dos.lib:
        @cd ../../hw/dos
        @./make.sh build lib
        @cd $(HERE)

dos86s/drvc.obj: drvc.c
        $(CC) $(CFLAGS) $[@

dos86s/drvci.obj: drvci.c
        $(CC) $(CFLAGS) -nt=_INITTEXT -nc=INITCODE $[@

link_objs = &
    dos86s/drva.obj &
    dos86s/drvc.obj &
    dos86s/drvci.obj

dos86s/drv.sys: $(link_objs)
        *wlink option quiet format dos com file {$(link_objs)} name dos86s/drv.sys option map=dos86s/drv.map

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
