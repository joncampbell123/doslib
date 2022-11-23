
OBJS =     tdchoose.obj

CC       = wcc
LINKER   = wcl
CFLAGS   = -zq -ms -s -bt=com -oilrtfm -fr=nul -wx -0 -fo=.obj -dSTDC -q

.c.obj:
	$(CC) $(CFLAGS) $[@

all: tdchoose.com

tdchoose.com: tdchoose.obj
	$(LINKER) -lr -l=com -q -fm=tdchoose.map -fe=tdchoose.com tdchoose.obj

clean: .SYMBOLIC
          del *.obj
          @echo Cleaning done
