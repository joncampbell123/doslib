all: linux-host linux-host/bmp2arr

linux-host:
	mkdir -p $@

linux-host/bmp2arr: bmp2arr.c
	gcc -DLINUX -Wall -g3 -O0 -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

