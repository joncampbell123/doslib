# GNU makefile, Linux host
all: linux-host linux-host/omfdump

linux-host:
	mkdir -p linux-host

linux-host/omfdump: linux-host/omfcstr.o linux-host/omfrec.o linux-host/omfdump.o
	gcc -o $@ $?

linux-host/omfrec.o: omfrec.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfcstr.o: omfcstr.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

linux-host/omfdump.o: omfdump.c
	gcc -DLINUX -Wall -Wextra -pedantic -std=gnu99 -c -o $@ $<

clean:
	rm -f linux-host/omfdump linux-host/*.o

