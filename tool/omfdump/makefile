# GNU makefile, Linux host
all: linux-host linux-host/omfdump

linux-host:
	mkdir -p linux-host

linux-host/omfdump: omfdump.c
	gcc -o $@ $<

clean:
	rm -f linux-host/omfdump

