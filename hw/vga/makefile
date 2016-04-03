
CC ?= gcc
CFLAGS ?= -Wall -std=gnu99

all: pcx2vrl pcxsscut vrl2vrs

vrl:
	./pcx2vrl -i 46113319.pcx -o 46113319.vrl -tc 0x0F -p 46113319.pal
	./pcx2vrl -i prussia.pcx  -o prussia.vrl  -tc 0x84 -p prussia.pal
	./pcx2vrl -i chikyuu.pcx  -o chikyuu.vrl  -tc 0x01 -p chikyuu.pal
	./pcx2vrl -i megaman.pcx  -o megaman.vrl           -p megaman.pal
	./pcx2vrl -i ed2.pcx      -o ed2.vrl      -tc 0xFF -p ed2.pal

prussiademo:
	cd dos86l && ../pcxsscut -s ../prussia.sht -hc prussia.h -hp demoanim_prussia_ -i ../prussia.pcx -p prussia.pal -tc 0x84 -y # run from subdirectory where output will not be committed accidentally
	cd dos86l && ../vrl2vrs -s ../prussia.sht -hc prussias.h -hp demoanim_prussia_ -o ../prussia.vrs

pcx2vrl: pcx2vrl.c
	$(CC) $(CFLAGS) -o $@ $<

vrl2vrs: vrl2vrs.c
	$(CC) $(CFLAGS) -o $@ $<

pcxsscut: pcxsscut.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -v pcx2vrl

