all: linux-host linux-host/usbacmtty

linux-host:
	mkdir -p $@

linux-host/usbacmtty: usbacmtty.c
	gcc -DLINUX -Wall -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

