# GNU makefile, Linux host
all: linux-host linux-host/omfsegfl

linux-host:
	mkdir -p linux-host

linux-host/omfsegfl: omfsegfl.c
	gcc -o $@ $<

clean:
	rm -f linux-host/omfsegfl

