all: linux-host linux-host/bmp2arr

linux-host:
	mkdir -p $@

linux-host/bmp2arr: bmp2arr.c
	gcc -DLINUX -Wall -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

