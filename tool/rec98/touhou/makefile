all: linux-host linux-host/bmp2arr

linux-host:
	mkdir -p $@

linux-host/bmp2arr: bmp2arr.c bmp2arrl.c
	gcc -DLINUX -Wall -g3 -O0 -Wextra -pedantic -o $@ bmp2arr.c bmp2arrl.c

clean:
	rm -Rf linux-host

