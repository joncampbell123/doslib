
CC ?= gcc
CFLAGS ?= -Wall -std=gnu99

all: pcx2vrl

vrl:
	./pcx2vrl -i 46113319.pcx -o 46113319.vrl -tc 0x0F -p 46113319.pal
	./pcx2vrl -i chikyuu.pcx  -o chikyuu.vrl  -tc 0x01 -p chikyuu.pal
	./pcx2vrl -i megaman.pcx  -o megaman.vrl           -p megaman.pal
	./pcx2vrl -i ed2.pcx      -o ed2.vrl      -tc 0xFF -p ed2.pal

pcx2vrl: pcx2vrl.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -v pcx2vrl

