# GNU makefile, Linux host
all: linux-host linux-host/omfdump

linux-host:
	mkdir -p linux-host

linux-host/omfdump: linux-host/omfdump.o
	gcc -o $@ $<

linux-host/omfdump.o: omfdump.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

clean:
	rm -f linux-host/omfdump linux-host/*.o

