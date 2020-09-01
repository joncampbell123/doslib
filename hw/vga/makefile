
CC ?= gcc
CFLAGS ?= -Wall -std=gnu99

all: pcx2vrl png2vrl pcxsscut vrl2vrs vrsdump vrldbg

vrl:
	./pcx2vrl -i 46113319.pcx -o 46113319.vrl -tc 0x0F -p 46113319.pal
	./pcx2vrl -i prussia.pcx  -o prussia.vrl  -tc 0x84 -p prussia.pal
	./pcx2vrl -i chikyuu.pcx  -o chikyuu.vrl  -tc 0x01 -p chikyuu.pal
	./pcx2vrl -i megaman.pcx  -o megaman.vrl           -p megaman.pal
	./pcx2vrl -i ed2.pcx      -o ed2.vrl      -tc 0xFF -p ed2.pal
	./png2vrl -i sorc1.png	  -o sorc1.vrl             -p sorc1.pal
	./png2vrl -i sorc2.png	  -o sorc2.vrl             -p sorc2.pal

prussiademo:
	cd dos86l && ../pcxsscut -s ../prussia.sht -hc prussia.h -hp demoanim_prussia_ -i ../prussia.pcx -p prussia.pal -tc 0x84 -y # run from subdirectory where output will not be committed accidentally
	cd dos86l && ../vrl2vrs -s ../prussia.sht -hc prussias.h -hp demoanim_prussia_ -o ../prussia.vrs # run from same subdirectory

pcx2vrl: pcx2vrl.c
	$(CC) $(CFLAGS) -o $@ $^

png2vrl: png2vrl.c
	$(CC) -lpng $(CFLAGS) -o $@ $^

comshtps.o: comshtps.c
	$(CC) $(CFLAGS) -c -o $@ $^

vrl2vrs.o: vrl2vrs.c
	$(CC) $(CFLAGS) -c -o $@ $^

vrl2vrs: vrl2vrs.o comshtps.o
	$(CC) $(CFLAGS) -o $@ $^

vrsdump: vrsdump.c
	$(CC) $(CFLAGS) -o $@ $^

vrldbg: vrldbg.c
	$(CC) $(CFLAGS) -o $@ $^

pcxsscut.o: pcxsscut.c
	$(CC) $(CFLAGS) -c -o $@ $^

pcxsscut: pcxsscut.o comshtps.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -fv pcx2vrl png2vrl pcxsscut vrl2vrs vrsdump vrldbg *.o

