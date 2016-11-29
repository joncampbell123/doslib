all: linux-host linux-host/expr1

linux-host:
	mkdir -p $@

linux-host/expr1: expr1.c
	gcc -DLINUX -Wall -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

