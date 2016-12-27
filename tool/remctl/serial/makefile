all: linux-host linux-host/remctlclient

linux-host:
	mkdir -p $@

linux-host/remctlclient: remctlclient.c
	gcc -DLINUX -Wall -Wextra -pedantic -o $@ $<

clean:
	rm -Rf linux-host

