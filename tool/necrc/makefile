all: linux-host linux-host/necrc

linux-host:
	mkdir -p $@

linux-host/necrc: necrc.c
	gcc -DLINUX -Wall -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

